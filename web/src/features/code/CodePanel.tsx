const SKETCH = `#include <Arduino.h>

constexpr int LED_PIN = 2;
constexpr int BUTTON_PIN = 0;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);
    Serial.println("SimForge ESP32-S3 DevKitC-1");
}

void loop() {
    const bool pressed = digitalRead(BUTTON_PIN) == LOW;
    digitalWrite(LED_PIN, pressed ? HIGH : LOW);
    Serial.printf("loop: botao=%d led=%d\\n", pressed, !pressed);
    delay(500);
}
`

const KEYWORDS =
  /\b(void|const|constexpr|int|bool|char|return|if|else|for|while|class|struct|namespace|using|auto|true|false|LOW|HIGH|OUTPUT|INPUT_PULLUP)\b/g

function highlight(line: string) {
  const parts: Array<{ text: string; kind: 'plain' | 'keyword' | 'comment' | 'string' | 'number' }> = []
  const trimmed = line.trimStart()
  if (trimmed.startsWith('//') || trimmed.startsWith('#')) {
    return [{ text: line, kind: trimmed.startsWith('//') ? 'comment' : 'keyword' }]
  }

  const tokens = line.split(/("[^"]*"|\b\d+\b)/g)
  for (const token of tokens) {
    if (!token) continue
    if (token.startsWith('"')) {
      parts.push({ text: token, kind: 'string' })
    } else if (/^\d+$/.test(token)) {
      parts.push({ text: token, kind: 'number' })
    } else {
      let last = 0
      for (const match of token.matchAll(KEYWORDS)) {
        const start = match.index ?? 0
        if (start > last) parts.push({ text: token.slice(last, start), kind: 'plain' })
        parts.push({ text: match[0], kind: 'keyword' })
        last = start + match[0].length
      }
      if (last < token.length) parts.push({ text: token.slice(last), kind: 'plain' })
    }
  }
  return parts
}

const CLASS: Record<string, string> = {
  plain: 'text-ink-muted',
  keyword: 'text-sky-300',
  comment: 'text-ink-dim',
  string: 'text-emerald-300',
  number: 'text-amber-300',
}

export function CodePanel() {
  const lines = SKETCH.split('\n')
  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center gap-2 border-b border-line px-3 py-1.5">
        <span className="rounded bg-shell-800 px-2 py-0.5 font-mono text-[10px] text-ink">
          src/main.cpp
        </span>
        <span className="font-mono text-[10px] text-ink-dim">platformio · arduino</span>
        <span className="ml-auto font-mono text-[10px] text-ok">compilado</span>
      </div>
      <div className="scroll-slim min-h-0 flex-1 overflow-auto py-2 font-mono text-[11px] leading-5">
        {lines.map((line, index) => (
          <div key={index} className="flex px-3 hover:bg-shell-800/40">
            <span className="w-8 shrink-0 text-right text-ink-dim select-none">{index + 1}</span>
            <span className="pl-3 whitespace-pre">
              {highlight(line).map((part, partIndex) => (
                <span key={partIndex} className={CLASS[part.kind]}>
                  {part.text}
                </span>
              ))}
            </span>
          </div>
        ))}
      </div>
    </div>
  )
}
