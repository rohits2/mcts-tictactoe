from sanic import Sanic
from sanic.response import json, html, redirect
from sanic.exceptions import NotFound, ServerError

app = Sanic()
app.static('/res/main.js', './res/main.js')
app.static('/res/material.css', './res/material.css')
app.static('/res/favicon.png', './res/favicon.png')
app.static('mcts/mcts.js', './mcts/mcts.js')
app.static('mcts/mcts.wasm', './mcts/mcts.wasm')
app.static('mcts/mcts.wasm.map', './mcts/mcts.wasm.map')
app.static('/lib/game.js', './lib/game.js')
app.static('/lib/grid.js', './lib/grid.js')
app.static('/lib/svg.js', './lib/svg.js')
with open("res/index.htm") as f:
    index_html = f.read()
with open("res/500.htm") as f:
    ise_html = f.read()
with open("res/404.htm") as f:
    notfound_html = f.read()

@app.route('/', methods=['GET'])
async def index(request):
    return html(index_html)

@app.exception(NotFound)
async def not_found(request, exception):
    print(exception)
    return html(notfound_html, status=exception.status_code)
        
@app.exception(ServerError)
async def ise(request, exception):
    return html(ise_html, status=exception.status_code)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=80)
