import { useState } from 'react'
import styles from './APIReference.module.css'

const SECTIONS = [
  {
    group: 'Router',
    items: [
      { sig: 'ExpressRouter* router_new()', desc: 'Allocate and return a new router. Returns NULL on allocation failure.' },
      { sig: 'int32_t router_add(router, route, method, handler)', desc: 'Register a handler for a route + method pair. Returns 0 on success, -1 on error (duplicate, capacity exceeded, or NULL args).' },
      { sig: 'int32_t router_add_middleware(router, handler)', desc: 'Attach a middleware function that runs before every route handler. Only one middleware is supported.' },
      { sig: 'void router_destroy(router)', desc: 'Free the router.' },
    ],
  },
  {
    group: 'Server',
    items: [
      { sig: 'ExpressServer* server_new(config, router)', desc: 'Create a server from a config and router. Scans public_path if set. Returns NULL on failure.' },
      { sig: 'void server_run(server)', desc: 'Start the event loop. Blocks until the process is killed.' },
      { sig: 'void server_destroy(server)', desc: 'Tear down the server, destroy the static file map, and free memory.' },
      { sig: 'size_t server_static_file_count(server)', desc: 'Return the number of files indexed from public_path at startup.' },
    ],
  },
  {
    group: 'Request',
    items: [
      { sig: 'param* get_request_param(req, key)', desc: 'Look up a URL query parameter by key (case-insensitive). Returns NULL if not found.' },
      { sig: 'param* get_request_route_param(req, key)', desc: 'Look up a route parameter by key.' },
      { sig: 'header* get_request_header(req, key)', desc: 'Look up a request header by name (case-insensitive).' },
      { sig: 'byte* get_request_body(req)', desc: 'Return a pointer to the raw request body. NULL if no body.' },
      { sig: 'size_t get_request_body_len(req)', desc: 'Return the body length in bytes.' },
      { sig: 'char* get_request_content_type(req)', desc: 'Return the Content-Type header value, or NULL.' },
    ],
  },
  {
    group: 'Response',
    items: [
      { sig: 'bool set_response_status(res, status)', desc: 'Set the HTTP status code string (e.g. "404"). Must be a valid 3-digit code 100–599.' },
      { sig: 'bool set_response_header(res, key, value)', desc: 'Add a response header. Headers are heap-allocated and freed after the response is written.' },
      { sig: 'bool set_response_body(res, body, len)', desc: 'Set the response body. The pointer is not copied — it must remain valid until the handler returns.' },
      { sig: 'header* get_response_header(res, key)', desc: 'Look up a previously set response header.' },
    ],
  },
  {
    group: 'Cookies',
    items: [
      { sig: 'char* get_cookie(req, key)', desc: 'Return the value of a request cookie by name (case-insensitive). Points into request storage — do not free.' },
      { sig: 'bool set_response_cookie(res, name, value)', desc: 'Add a Set-Cookie header to the response. Name and value are copied.' },
    ],
  },
]

export default function APIReference() {
  const [open, setOpen] = useState('Router')

  return (
    <section id="api">
      <p className="section-label">API Reference</p>
      <h2 className="section-title">The full surface area.</h2>
      <p className="section-desc">
        ExpressC exposes a small, stable C API. Every function is in <code>ExpressC.h</code>.
      </p>
      <div className={styles.layout}>
        <nav className={styles.sidebar}>
          {SECTIONS.map(s => (
            <button
              key={s.group}
              className={`${styles.tab} ${open === s.group ? styles.active : ''}`}
              onClick={() => setOpen(s.group)}
            >
              {s.group}
            </button>
          ))}
        </nav>
        <div className={styles.content}>
          {SECTIONS.filter(s => s.group === open).map(s => (
            <ul key={s.group} className={styles.list}>
              {s.items.map(item => (
                <li key={item.sig} className={styles.item}>
                  <code className={styles.sig}>{item.sig}</code>
                  <p className={styles.desc}>{item.desc}</p>
                </li>
              ))}
            </ul>
          ))}
        </div>
      </div>
    </section>
  )
}
