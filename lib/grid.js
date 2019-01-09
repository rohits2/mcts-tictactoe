var GRID_WIDTH = 30;
var SUPERGRID_WIDTH = 1.25 * GRID_WIDTH * 3;
var INTERIOR_OFFSET = (SUPERGRID_WIDTH - 3 * GRID_WIDTH) / 2;
var MARKING_OFFSET = 3;
var MAX_DIM = SUPERGRID_WIDTH * 3;
var INTERIOR_BORDER_COLOR = '#AAAAAA';
var INTERIOR_BORDER_WIDTH = 2.0;
var EXTERIOR_BORDER_COLOR = '#EEEEEE';
var EXTERIOR_BORDER_WIDTH = 2.0;
var X_COLOR = '#FF9933';
var O_COLOR = '#3399FF';
var LINK_COLOR = '#009933';
var SMALL_SYM_WIDTH = 1.0;
var BIG_SYM_WIDTH = 4.0;
function drawCell(x, y, width, line_width, line_color) {
    return [
        new Line(x + width, y, x + width, y + 3 * width, line_color, line_width),
        new Line(x + 2 * width, y, x + 2 * width, y + 3 * width, line_color, line_width),
        new Line(x, y + width, x + 3 * width, y + width, line_color, line_width),
        new Line(x, y + 2 * width, x + 3 * width, y + 2 * width, line_color, line_width),
    ];
}
var Grid = /** @class */ (function () {
    function Grid() {
        var _a, _b;
        this.svg = new SVG(MAX_DIM, MAX_DIM);
        for (var i = 0; i < 3; i++) {
            for (var j = 0; j < 3; j++) {
                (_a = this.svg).add.apply(_a, drawCell(i * SUPERGRID_WIDTH + INTERIOR_OFFSET, j * SUPERGRID_WIDTH + INTERIOR_OFFSET, GRID_WIDTH, INTERIOR_BORDER_WIDTH, INTERIOR_BORDER_COLOR));
            }
        }
        (_b = this.svg).add.apply(_b, drawCell(0, 0, SUPERGRID_WIDTH, EXTERIOR_BORDER_WIDTH, EXTERIOR_BORDER_COLOR));
    }
    Grid.prototype.drawSmallX = function (i, j, ii, jj) {
        var x = j * SUPERGRID_WIDTH + jj * GRID_WIDTH + INTERIOR_OFFSET +
            MARKING_OFFSET;
        var y = i * SUPERGRID_WIDTH + ii * GRID_WIDTH + INTERIOR_OFFSET +
            MARKING_OFFSET;
        this.svg.add(new Line(x, y, x + GRID_WIDTH - 2 * MARKING_OFFSET, y + GRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, SMALL_SYM_WIDTH));
        this.svg.add(new Line(x + GRID_WIDTH - 2 * MARKING_OFFSET, y, x, y + GRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, SMALL_SYM_WIDTH));
    };
    Grid.prototype.drawSmallO = function (i, j, ii, jj) {
        var x = j * SUPERGRID_WIDTH + jj * GRID_WIDTH + INTERIOR_OFFSET +
            GRID_WIDTH / 2;
        var y = i * SUPERGRID_WIDTH + ii * GRID_WIDTH + INTERIOR_OFFSET +
            GRID_WIDTH / 2;
        this.svg.add(new Circle(x, y, GRID_WIDTH / 2 - MARKING_OFFSET, O_COLOR, SMALL_SYM_WIDTH));
    };
    Grid.prototype.addSmallLink = function (i, j, ii, jj, callback) {
        var x = j * SUPERGRID_WIDTH + jj * GRID_WIDTH + INTERIOR_OFFSET;
        var y = i * SUPERGRID_WIDTH + ii * GRID_WIDTH + INTERIOR_OFFSET;
        this.svg.add(new ClickableCaller(x, y, GRID_WIDTH, GRID_WIDTH, callback, LINK_COLOR, 0.1));
    };
    Grid.prototype.drawBigX = function (i, j) {
        var x = j * SUPERGRID_WIDTH + MARKING_OFFSET;
        var y = i * SUPERGRID_WIDTH + MARKING_OFFSET;
        this.svg.add(new Line(x, y, x + SUPERGRID_WIDTH - 2 * MARKING_OFFSET, y + SUPERGRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, BIG_SYM_WIDTH));
        this.svg.add(new Line(x + SUPERGRID_WIDTH - 2 * MARKING_OFFSET, y, x, y + SUPERGRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, BIG_SYM_WIDTH));
    };
    Grid.prototype.drawBigO = function (i, j) {
        var x = j * SUPERGRID_WIDTH + SUPERGRID_WIDTH / 2;
        var y = i * SUPERGRID_WIDTH + SUPERGRID_WIDTH / 2;
        this.svg.add(new Circle(x, y, SUPERGRID_WIDTH / 2 - MARKING_OFFSET, O_COLOR, BIG_SYM_WIDTH));
    };
    Grid.prototype.render = function () {
        return this.svg.render();
    };
    return Grid;
}());
