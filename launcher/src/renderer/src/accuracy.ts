const percent = new Intl.NumberFormat(undefined, { style: 'percent', maximumFractionDigits: 1 })

export function formatAccuracy(ratio: number | null): string {
  if (ratio === null) return '—'
  if (ratio > 1) return `${percent.format(1)}+`
  return percent.format(ratio === 1 ? 1 : Math.min(ratio, 0.999))
}
