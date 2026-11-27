const dateTime = new Intl.DateTimeFormat(undefined, {
  calendar: 'gregory',
  dateStyle: 'medium',
  timeStyle: 'short',
})

export function formatAtreyanDateTime(timestamp: string | null): string | null {
  if (!timestamp) return null
  const date = new Date(timestamp)
  if (Number.isNaN(date.getTime())) return null

  const atreyanYear = `${date.getFullYear() - 2020}AE`
  return dateTime
    .formatToParts(date)
    .map(({ type, value }) => (type === 'year' ? atreyanYear : value))
    .join('')
}
