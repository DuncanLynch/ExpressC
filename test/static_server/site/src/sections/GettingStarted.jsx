import CodeBlock from '../components/CodeBlock'
import styles from './GettingStarted.module.css'

const BASIC = `#include <stdio.h>
#include "ExpressC.h"

static void hello(void* ctx, http_request* req, http_response* res) {
    (void)ctx; (void)req;
    static const char body[] = "Hello, world!\\n";
    set_response_header(res, "Content-Type", "text/plain; charset=utf-8");
    set_response_body(res, (const byte*)body, sizeof(body) - 1);
}

int main(void) {
    ExpressRouter* router = router_new();
    router_add(router, "/", GET, hello);

    ExpressConfig cfg = { .port = 8080 };
    ExpressServer* server = server_new(&cfg, router);

    printf("Listening on http://localhost:8080\\n");
    server_run(server);

    server_destroy(server);
    router_destroy(router);
    return 0;
}`

const STATIC = `ExpressConfig cfg = {
    .port        = 8080,
    .public_path = "./public",
    .spa_mode    = true,   /* serve index.html for unmatched routes */
};`

const COOKIES = `static void handler(void* ctx, http_request* req, http_response* res) {
    char* session = get_cookie(req, "session");
    if (session == NULL) {
        set_response_cookie(res, "session", "abc123");
    }
}`

const steps = [
  {
    step: '01',
    title: 'Clone and build',
    lang: 'bash',
    code: `git clone https://github.com/DuncanLynch/ExpressC
cd ExpressC
gcc -O2 -o my_server my_server.c ExpressC.c TCPServer/TCPServer.c`,
  },
  {
    step: '02',
    title: 'Write a server',
    lang: 'c',
    code: BASIC,
  },
  {
    step: '03',
    title: 'Serve static files',
    lang: 'c',
    code: STATIC,
  },
  {
    step: '04',
    title: 'Use cookies',
    lang: 'c',
    code: COOKIES,
  },
]

export default function GettingStarted() {
  return (
    <section id="getting-started">
      <p className="section-label">Getting Started</p>
      <h2 className="section-title">Up and running in minutes.</h2>
      <p className="section-desc">
        No package manager. No build system. Just include two C files and compile.
      </p>
      <div className={styles.steps}>
        {steps.map(s => (
          <div key={s.step} className={styles.step}>
            <div className={styles.stepHeader}>
              <span className={styles.stepNum}>{s.step}</span>
              <h3 className={styles.stepTitle}>{s.title}</h3>
            </div>
            <CodeBlock code={s.code} lang={s.lang} />
          </div>
        ))}
      </div>
    </section>
  )
}
