import { useState, useEffect, useRef } from 'react'
import styles from './Demo.module.css'

function Terminal({ entries }) {
  const ref = useRef(null)
  useEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight
  }, [entries])

  return (
    <div className={styles.terminal} ref={ref}>
      {entries.length === 0 && (
        <span className={styles.placeholder}>// responses will appear here</span>
      )}
      {entries.map((e, i) => (
        <div key={i} className={styles.entry}>
          <span className={styles.method}>{e.method}</span>
          <span className={styles.path}>{e.path}</span>
          <span className={`${styles.status} ${e.ok ? styles.ok : styles.err}`}>
            {e.status}
          </span>
          <pre className={styles.body}>{e.body}</pre>
        </div>
      ))}
    </div>
  )
}

function StatCounter({ label, value }) {
  const [displayed, setDisplayed] = useState(0)
  const prev = useRef(0)

  useEffect(() => {
    if (value === null) return
    const start = prev.current
    const end = value
    const diff = end - start
    if (diff === 0) return
    const steps = Math.min(Math.abs(diff), 20)
    let i = 0
    const id = setInterval(() => {
      i++
      setDisplayed(Math.round(start + (diff * i) / steps))
      if (i >= steps) {
        clearInterval(id)
        prev.current = end
      }
    }, 30)
    return () => clearInterval(id)
  }, [value])

  return (
    <div className={styles.stat}>
      <span className={styles.statValue}>{value === null ? '—' : displayed}</span>
      <span className={styles.statLabel}>{label}</span>
    </div>
  )
}

export default function Demo() {
  const [log, setLog] = useState([])
  const [echoInput, setEchoInput] = useState('')
  const [stats, setStats] = useState(null)
  const [loading, setLoading] = useState({})

  const addEntry = (entry) => setLog(l => [...l, entry])

  const setLoad = (key, val) => setLoading(l => ({ ...l, [key]: val }))

  async function fetchStats() {
    try {
      const res = await fetch('/api/stats')
      const data = await res.json()
      setStats(data)
    } catch {
      setStats(null)
    }
  }

  useEffect(() => {
    fetchStats()
    const id = setInterval(fetchStats, 2000)
    return () => clearInterval(id)
  }, [])

  async function doPing() {
    setLoad('ping', true)
    try {
      const res = await fetch('/api/ping')
      const body = await res.text()
      addEntry({ method: 'GET', path: '/api/ping', status: res.status, ok: res.ok, body })
    } catch (e) {
      addEntry({ method: 'GET', path: '/api/ping', status: 'ERR', ok: false, body: e.message })
    }
    setLoad('ping', false)
    fetchStats()
  }

  async function doEcho() {
    setLoad('echo', true)
    try {
      const res = await fetch('/api/echo', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: echoInput || '(empty)',
      })
      const body = await res.text()
      addEntry({ method: 'POST', path: '/api/echo', status: res.status, ok: res.ok, body })
    } catch (e) {
      addEntry({ method: 'POST', path: '/api/echo', status: 'ERR', ok: false, body: e.message })
    }
    setLoad('echo', false)
    fetchStats()
  }

  return (
    <section id="demo">
      <p className="section-label">Interactive Demo</p>
      <h2 className="section-title">Talk to the server.</h2>
      <p className="section-desc">
        These buttons hit real API endpoints on this ExpressC server. The stats update live.
      </p>

      <div className={styles.stats}>
        <StatCounter label="requests served" value={stats?.requests ?? null} />
        <StatCounter label="files indexed"   value={stats?.files    ?? null} />
      </div>

      <div className={styles.controls}>
        <div className={styles.row}>
          <button
            className={styles.btn}
            onClick={doPing}
            disabled={loading.ping}
          >
            {loading.ping ? '...' : 'GET /api/ping'}
          </button>
        </div>

        <div className={styles.row}>
          <input
            className={styles.input}
            placeholder="Type something to echo..."
            value={echoInput}
            onChange={e => setEchoInput(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && doEcho()}
          />
          <button
            className={styles.btn}
            onClick={doEcho}
            disabled={loading.echo}
          >
            {loading.echo ? '...' : 'POST /api/echo'}
          </button>
        </div>

        <button
          className={`${styles.btn} ${styles.clear}`}
          onClick={() => setLog([])}
        >
          Clear
        </button>
      </div>

      <Terminal entries={log} />
    </section>
  )
}
