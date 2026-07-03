import styles from './Features.module.css'

const FEATURES = [
  {
    icon: '⚡',
    title: 'Fast',
    desc: 'Built on a non-blocking epoll TCP server. Handles keep-alive connections and pipelined requests.',
  },
  {
    icon: '🔀',
    title: 'Routing',
    desc: 'Register handlers per route and method. HEAD is handled automatically for any GET route.',
  },
  {
    icon: '🍪',
    title: 'Cookies',
    desc: 'Read cookies from requests and set them on responses with a simple API.',
  },
  {
    icon: '🗂',
    title: 'Static Files',
    desc: 'Serve a public directory with startup-time indexing and O(1) hash map lookups.',
  },
  {
    icon: '🔗',
    title: 'Middleware',
    desc: 'Attach a single middleware function that runs before every route handler.',
  },
  {
    icon: '🔒',
    title: 'Safe',
    desc: 'No path traversal. Static files are indexed at startup — only pre-scanned files can be served.',
  },
]

export default function Features() {
  return (
    <section id="features">
      <p className="section-label">Why ExpressC</p>
      <h2 className="section-title">Everything you need, nothing you don't.</h2>
      <p className="section-desc">
        ExpressC gives you a familiar Express.js-style API with no runtime, no garbage collector, and no dependencies beyond libc.
      </p>
      <div className={styles.grid}>
        {FEATURES.map(f => (
          <div key={f.title} className={styles.card}>
            <span className={styles.icon}>{f.icon}</span>
            <h3 className={styles.name}>{f.title}</h3>
            <p className={styles.desc}>{f.desc}</p>
          </div>
        ))}
      </div>
    </section>
  )
}
