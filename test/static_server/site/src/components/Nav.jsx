import { useState, useEffect } from 'react'
import styles from './Nav.module.css'

const links = [
  { label: 'Features',        href: '#features' },
  { label: 'Getting Started', href: '#getting-started' },
  { label: 'API',             href: '#api' },
  { label: 'Demo',            href: '#demo' },
]

export default function Nav() {
  const [scrolled, setScrolled] = useState(false)

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 40)
    window.addEventListener('scroll', onScroll, { passive: true })
    return () => window.removeEventListener('scroll', onScroll)
  }, [])

  return (
    <nav className={`${styles.nav} ${scrolled ? styles.scrolled : ''}`}>
      <a href="#hero" className={styles.logo}>ExpressC</a>
      <ul className={styles.links}>
        {links.map(l => (
          <li key={l.href}>
            <a href={l.href}>{l.label}</a>
          </li>
        ))}
      </ul>
      <a
        href="https://github.com/DuncanLynch/ExpressC"
        target="_blank"
        rel="noreferrer"
        className={styles.github}
      >
        GitHub
      </a>
    </nav>
  )
}
