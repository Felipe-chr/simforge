import type { ReactElement, SVGProps } from 'react'

type IconProps = SVGProps<SVGSVGElement>

function base(props: IconProps) {
  return {
    viewBox: '0 0 24 24',
    fill: 'none',
    stroke: 'currentColor',
    strokeWidth: 1.7,
    strokeLinecap: 'round' as const,
    strokeLinejoin: 'round' as const,
    width: 16,
    height: 16,
    ...props,
  }
}

export function PlayIcon(props: IconProps) {
  return (
    <svg {...base(props)} fill="currentColor" stroke="none">
      <path d="M8 5.5v13l11-6.5z" />
    </svg>
  )
}

export function PauseIcon(props: IconProps) {
  return (
    <svg {...base(props)} fill="currentColor" stroke="none">
      <rect x="6.5" y="5" width="3.6" height="14" rx="1" />
      <rect x="13.9" y="5" width="3.6" height="14" rx="1" />
    </svg>
  )
}

export function StopIcon(props: IconProps) {
  return (
    <svg {...base(props)} fill="currentColor" stroke="none">
      <rect x="6.5" y="6.5" width="11" height="11" rx="1.6" />
    </svg>
  )
}

export function StepIcon(props: IconProps) {
  return (
    <svg {...base(props)} fill="currentColor" stroke="none">
      <path d="M7 5.5v13l8.5-6.5z" />
      <rect x="16.6" y="5.5" width="2.4" height="13" rx="1" />
    </svg>
  )
}

export function ResetIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <path d="M4.5 10a7.5 7.5 0 1 1 2.2 5.3" />
      <path d="M4.2 5.6V10h4.4" />
    </svg>
  )
}

export function SearchIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <circle cx="11" cy="11" r="6.2" />
      <path d="m15.6 15.6 3.6 3.6" />
    </svg>
  )
}

export function PlusIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <path d="M12 5v14M5 12h14" />
    </svg>
  )
}

export function RotateIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <path d="M20 11.5A8 8 0 1 0 12 20" />
      <path d="M20 5.5v6h-6" />
    </svg>
  )
}

export function TrashIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <path d="M4.5 7h15M9.5 7V5.2h5V7M6.5 7l.8 12.2h9.4L17.5 7" />
      <path d="M10.5 10.5v5.5M13.5 10.5v5.5" />
    </svg>
  )
}

export function ZoomInIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <circle cx="11" cy="11" r="6.2" />
      <path d="M11 8.4v5.2M8.4 11h5.2M15.6 15.6 19.2 19.2" />
    </svg>
  )
}

export function ZoomOutIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <circle cx="11" cy="11" r="6.2" />
      <path d="M8.4 11h5.2M15.6 15.6 19.2 19.2" />
    </svg>
  )
}

export function ChipIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <rect x="7" y="7" width="10" height="10" rx="1.6" />
      <path d="M10 3.6v3.2M14 3.6v3.2M10 17.2v3.2M14 17.2v3.2M3.6 10h3.2M3.6 14h3.2M17.2 10h3.2M17.2 14h3.2" />
    </svg>
  )
}

export function UsbIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <path d="M12 3.5v11" />
      <path d="m12 3.5 3 3.4M12 3.5 9 6.9" />
      <circle cx="12" cy="16.6" r="2.4" />
      <path d="M12 14.2V9.8" />
      <path d="M12 9.8H8.4v2.2" />
      <rect x="6.4" y="12" width="4" height="3" rx="0.8" />
    </svg>
  )
}

export function LedIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <circle cx="11" cy="10.5" r="5" />
      <path d="M11 15.5v4M7.8 17.4l6.4-6.4" />
    </svg>
  )
}

export function ButtonIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <rect x="4" y="9" width="16" height="7" rx="2.2" />
      <circle cx="12" cy="12.5" r="2.2" />
    </svg>
  )
}

export function ResistorIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <path d="M3 12h4l1.4-3 2.2 6 2.2-6 2.2 6 1.4-3h4.6" />
    </svg>
  )
}

export function KnobIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <circle cx="12" cy="12" r="7" />
      <path d="M12 6.6v4.2" />
    </svg>
  )
}

export function SpeakerIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <path d="M5 10.5v3h3l4 3.4V7.1L8 10.5z" />
      <path d="M15 9.4a3.6 3.6 0 0 1 0 5.2M17.4 7.2a7 7 0 0 1 0 9.6" />
    </svg>
  )
}

export function DisplayIcon(props: IconProps) {
  return (
    <svg {...base(props)}>
      <rect x="3.4" y="5.4" width="17.2" height="11.2" rx="1.8" />
      <path d="M8 20h8" />
    </svg>
  )
}

export const DEVICE_ICONS: Record<string, (props: IconProps) => ReactElement> = {
  'simforge.led': LedIcon,
  'simforge.button': ButtonIcon,
  'simforge.resistor': ResistorIcon,
  'simforge.potentiometer': KnobIcon,
  'simforge.buzzer': SpeakerIcon,
  'simforge.oled': DisplayIcon,
}
