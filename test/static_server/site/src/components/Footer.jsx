import styles from './Footer.module.css'

export default function Footer() {
  return (
    <footer className={styles.footer}>
      <span>ExpressC — MIT License</span>
      <a href="https://github.com/DuncanLynch/ExpressC" target="_blank" rel="noreferrer">
        GitHub
      </a>
    </footer>
  )
}
