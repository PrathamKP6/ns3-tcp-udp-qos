import csv
import html
from pathlib import Path

root = Path("results")
raw = root / "csv"
out = root / "graphs"
out.mkdir(parents=True, exist_ok=True)

files = [
    ("01-tcp-baseline.csv", "TCP only"),
    ("02-udp-baseline.csv", "UDP only"),
    ("03-combined-light.csv", "Light"),
    ("04-combined-moderate.csv", "Moderate"),
    ("05-combined-heavy.csv", "Heavy"),
    ("07-qos-prioritized.csv", "QoS Prioritized"),
]

rows = []
for filename, label in files:
    with (raw / filename).open(newline="") as f:
        row = next(csv.DictReader(f))
    row["label"] = label
    rows.append(row)

with (raw / "comparison-summary.csv").open("w", newline="") as f:
    fields = [
        "label", "scenario", "udp_rate", "tcp_rx_bytes",
        "tcp_goodput_mbps", "tcp_completion_seconds", "tcp_complete",
        "udp_tx_packets", "udp_rx_packets", "udp_delivery_percent",
        "udp_loss_percent", "udp_mean_delay_ms"
    ]
    writer = csv.DictWriter(f, fieldnames=fields)
    writer.writeheader()
    for row in rows:
        writer.writerow({k: row.get(k, "") for k in fields})

def svg_graph(filename, title, ylabel, points, ymax=None):
    # points = [(label, value_or_None), ...]
    W, H = 1100, 650
    left, right, top, bottom = 110, 40, 100, 150
    plot_w, plot_h = W-left-right, H-top-bottom
    values = [v for _, v in points if v is not None]
    if not values:
        return
    maxval = ymax if ymax is not None else max(values) * 1.15
    if maxval <= 0:
        maxval = 1

    def y(v):
        return top + plot_h - (v/maxval)*plot_h

    def esc(s):
        return html.escape(str(s))

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{W/2}" y="45" text-anchor="middle" font-family="Arial" font-size="26" font-weight="bold">{esc(title)}</text>',
    ]

    for i in range(6):
        val = maxval * i / 5
        yy = y(val)
        parts.append(f'<line x1="{left}" y1="{yy}" x2="{W-right}" y2="{yy}" stroke="#dddddd"/>')
        parts.append(f'<text x="{left-15}" y="{yy+5}" text-anchor="end" font-family="Arial" font-size="15">{val:.1f}</text>')

    parts.append(f'<line x1="{left}" y1="{top}" x2="{left}" y2="{H-bottom}" stroke="#333"/>')
    parts.append(f'<line x1="{left}" y1="{H-bottom}" x2="{W-right}" y2="{H-bottom}" stroke="#333"/>')
    parts.append(f'<text x="25" y="{H/2}" transform="rotate(-90 25 {H/2})" text-anchor="middle" font-family="Arial" font-size="18">{esc(ylabel)}</text>')

    n = len(points)
    slot = plot_w / n
    bar_w = min(95, slot*0.52)
    for i, (label, val) in enumerate(points):
        x = left + slot*(i+0.5)
        if val is not None:
            yy = y(val)
            hh = H-bottom-yy
            parts.append(f'<rect x="{x-bar_w/2}" y="{yy}" width="{bar_w}" height="{hh}" fill="#3977b8"/>')
            parts.append(f'<text x="{x}" y="{yy-10}" text-anchor="middle" font-family="Arial" font-size="15">{val:.2f}</text>')
        else:
            parts.append(f'<text x="{x}" y="{H-bottom-20}" text-anchor="middle" font-family="Arial" font-size="15">N/A</text>')
        parts.append(f'<text x="{x}" y="{H-bottom+30}" text-anchor="middle" font-family="Arial" font-size="14">{esc(label)}</text>')

    parts.append(f'<text x="{W/2}" y="{H-35}" text-anchor="middle" font-family="Arial" font-size="16">Experiment scenario</text>')
    parts.append('</svg>')
    (out / filename).write_text("\n".join(parts), encoding="utf-8")

labels = [r["label"] for r in rows]

def number(row, key):
    try:
        return float(row[key])
    except (ValueError, TypeError, KeyError):
        return None

tcp_completion = []
for r in rows:
    complete = r["tcp_complete"] == "1"
    val = number(r, "tcp_completion_seconds")
    tcp_completion.append((r["label"], val if complete and val is not None and val >= 0 else None))

svg_graph(
    "tcp-completion-time.svg",
    "TCP Transfer Completion Time",
    "Seconds",
    tcp_completion
)

svg_graph(
    "tcp-goodput.svg",
    "TCP Application Goodput",
    "Mbps",
    [(r["label"], number(r, "tcp_goodput_mbps")) for r in rows]
)

svg_graph(
    "udp-delivery-ratio.svg",
    "UDP Packet Delivery Ratio",
    "Delivery (%)",
    [(r["label"], number(r, "udp_delivery_percent") if int(r["udp_tx_packets"]) > 0 else None) for r in rows],
    ymax=100
)

svg_graph(
    "udp-loss-ratio.svg",
    "UDP Estimated Packet Loss Ratio",
    "Loss (%)",
    [(r["label"], number(r, "udp_loss_percent") if int(r["udp_tx_packets"]) > 0 else None) for r in rows],
    ymax=100
)

svg_graph(
    "udp-mean-delay.svg",
    "UDP Mean FlowMonitor Delay",
    "Milliseconds",
    [(r["label"], number(r, "udp_mean_delay_ms") if int(r["udp_rx_packets"]) > 0 else None) for r in rows]
)

print("Graphs created:")
for p in sorted(out.glob("*.svg")):
    print(f"  {p} ({p.stat().st_size} bytes)")
print(f"Summary CSV: {raw / 'comparison-summary.csv'}")
