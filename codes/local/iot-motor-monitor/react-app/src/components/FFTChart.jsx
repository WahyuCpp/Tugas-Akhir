import React from "react";
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  Legend,
} from "recharts";

// axisData: { freqs: number[], magnitude: number[], dominant_freq, dominant_magnitude }
export default function FFTChart({ axisData, axisLabel }) {
  if (!axisData) {
    return <div style={{ color: "#64748b" }}>No data for axis {axisLabel}</div>;
  }

  const chartData = axisData.freqs.map((f, i) => ({
    freq: Number(f.toFixed(1)),
    magnitude: axisData.magnitude[i],
  }));

  return (
    <div style={{ marginBottom: "24px" }}>
      <h4 style={{ color: "#e2e8f0", marginBottom: "4px" }}>
        Axis {axisLabel.toUpperCase()} — dominant: {axisData.dominant_freq.toFixed(1)} Hz
      </h4>
      <ResponsiveContainer width="100%" height={200}>
        <LineChart data={chartData}>
          <CartesianGrid strokeDasharray="3 3" stroke="#334155" />
          <XAxis dataKey="freq" stroke="#94a3b8" label={{ value: "Hz", position: "insideBottomRight", offset: -5, fill: "#94a3b8" }} />
          <YAxis stroke="#94a3b8" />
          <Tooltip contentStyle={{ background: "#1e293b", border: "none" }} />
          <Legend />
          <Line type="monotone" dataKey="magnitude" stroke="#2563eb" dot={false} name={`${axisLabel} magnitude`} />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
