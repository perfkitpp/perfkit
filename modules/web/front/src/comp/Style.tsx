export default function findValueStyle(value: any): [string, string | `rgb(${number}, ${number}, ${number})` | `#${string}`] {
  switch (typeof value) {
    case "object":
      if (value instanceof Array)
        return ["ri-brackets-line", '#bba962'];
      else
        return ["ri-braces-line", '#0bab7e'];
    case "boolean":
      return ["ri-checkbox-line", '#0184cb'];
    case "number":
    case "bigint":
      return ["ri-hashtag", '#109675'];
    case "string":
      return ["ri-text", '#ea6a21'];
    default:
      return ["", '#123212'];
  }
}