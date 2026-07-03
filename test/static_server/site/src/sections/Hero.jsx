import { useEffect, useState } from 'react'
import styles from './Hero.module.css'

const TAGLINES = [
  'A lightweight HTTP server library written in C.',
  'Routes, middleware, cookies — zero dependencies.',
  'Express-inspired. No runtime. No garbage collector.',
]

export default function Hero() {
  const [lineIndex, setLineIndex] = useState(0)
  const [displayed, setDisplayed] = useState('')
  const [typing, setTyping] = useState(true)

  useEffect(() => {
    const target = TAGLINES[lineIndex]
    if (typing) {
      if (displayed.length < target.length) {
        const t = setTimeout(() => setDisplayed(target.slice(0, displayed.length + 1)), 28)
        return () => clearTimeout(t)
      } else {
        const t = setTimeout(() => setTyping(false), 2200)
        return () => clearTimeout(t)
      }
    } else {
      if (displayed.length > 0) {
        const t = setTimeout(() => setDisplayed(displayed.slice(0, -1)), 14)
        return () => clearTimeout(t)
      } else {
        setLineIndex(i => (i + 1) % TAGLINES.length)
        setTyping(true)
      }
    }
  }, [displayed, typing, lineIndex])

  return (
    <section id="hero" className={styles.hero}>
      <div className={styles.badge}>v1.1 — MIT License</div>
      <h1 className={styles.title}>
        Express<span className={styles.c}>C</span>
      </h1>
      <p className={styles.tagline}>
        {displayed}
        <span className={styles.cursor}>|</span>
      </p>
      <div className={styles.actions}>
        <a href="#getting-started" className={styles.primary}>Get started</a>
        <a href="#demo" className={styles.secondary}>Try the demo</a>
      </div>
    </section>
  )
}
