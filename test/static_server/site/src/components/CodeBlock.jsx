import { useEffect, useRef } from 'react'
import hljs from 'highlight.js/lib/core'
import c from 'highlight.js/lib/languages/c'
import bash from 'highlight.js/lib/languages/bash'
import 'highlight.js/styles/github-dark.css'
import styles from './CodeBlock.module.css'

hljs.registerLanguage('c', c)
hljs.registerLanguage('bash', bash)

export default function CodeBlock({ code, lang = 'c' }) {
  const ref = useRef(null)

  useEffect(() => {
    if (ref.current) {
      ref.current.removeAttribute('data-highlighted')
      hljs.highlightElement(ref.current)
    }
  }, [code, lang])

  return (
    <div className={styles.wrap}>
      <pre className={styles.pre}>
        <code ref={ref} className={`language-${lang}`}>
          {code}
        </code>
      </pre>
    </div>
  )
}
