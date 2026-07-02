const express = require('express');

const app = express();
let lastMessage = 'First Message!';

app.use(express.raw({ type: '*/*', limit: '1mb' }));

app.get('/', (req, res) => {
    res.set('Content-Type', 'text/plain; charset=utf-8');
    res.send('ExpressC test server\nGET /ping\nPOST /echo\n');
});

app.get('/ping', (req, res) => {
    res.set('Content-Type', 'text/plain; charset=utf-8');
    res.send('pong\n');
});

app.post('/echo', (req, res) => {
    const ct = req.get('Content-Type') || 'application/octet-stream';
    res.set('Content-Type', ct);
    res.send(req.body);
});

app.get('/message', (req, res) => {
    res.send(lastMessage);
});

app.post('/message', (req, res) => {
    const body = req.body;
    if (body.length > 255) {
        res.status(400).end();
        return;
    }
    lastMessage = body.toString();
    res.status(200).end();
});

const port = parseInt(process.argv[2]) || parseInt(process.env.PORT) || 8080;
app.listen(port, '0.0.0.0', () => {
    process.stderr.write(`Express.js test server listening on 0.0.0.0:${port}\n`);
});
