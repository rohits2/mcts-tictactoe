const GRID_WIDTH = 30
const SUPERGRID_WIDTH = 1.25 * GRID_WIDTH * 3
const INTERIOR_OFFSET = (SUPERGRID_WIDTH - 3 * GRID_WIDTH) / 2
const MARKING_OFFSET = 3
const MAX_DIM = SUPERGRID_WIDTH * 3

const INTERIOR_BORDER_COLOR = '#AAAAAA'
const INTERIOR_BORDER_WIDTH = 2.0
const EXTERIOR_BORDER_COLOR = '#EEEEEE'
const EXTERIOR_BORDER_WIDTH = 2.0
const X_COLOR = '#FF9933'
const O_COLOR = '#3399FF'
const LINK_COLOR = '#009933'
const SMALL_SYM_WIDTH = 1.0
const BIG_SYM_WIDTH = 4.0

function drawCell(x, y, width, line_width, line_color) {
  return [
    new Line(x + width, y, x + width, y + 3 * width, line_color, line_width),
    new Line(
        x + 2 * width, y, x + 2 * width, y + 3 * width, line_color, line_width),
    new Line(x, y + width, x + 3 * width, y + width, line_color, line_width),
    new Line(
        x, y + 2 * width, x + 3 * width, y + 2 * width, line_color, line_width),
  ]
}

class Grid {
  svg: SVG;
  constructor() {
    this.svg = new SVG(MAX_DIM, MAX_DIM);
    for (let i = 0; i < 3; i++) {
      for (let j = 0; j < 3; j++) {
        this.svg.add(...drawCell(
            i * SUPERGRID_WIDTH + INTERIOR_OFFSET,
            j * SUPERGRID_WIDTH + INTERIOR_OFFSET, GRID_WIDTH,
            INTERIOR_BORDER_WIDTH, INTERIOR_BORDER_COLOR));
      }
    }
    this.svg.add(...drawCell(
        0, 0, SUPERGRID_WIDTH, EXTERIOR_BORDER_WIDTH, EXTERIOR_BORDER_COLOR))
  }
  drawSmallX(i: number, j: number, ii: number, jj: number) {
    let x = j * SUPERGRID_WIDTH + jj * GRID_WIDTH + INTERIOR_OFFSET +
        MARKING_OFFSET;
    let y = i * SUPERGRID_WIDTH + ii * GRID_WIDTH + INTERIOR_OFFSET +
        MARKING_OFFSET;
    this.svg.add(new Line(
        x, y, x + GRID_WIDTH - 2 * MARKING_OFFSET,
        y + GRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, SMALL_SYM_WIDTH));
    this.svg.add(new Line(
        x + GRID_WIDTH - 2 * MARKING_OFFSET, y, x,
        y + GRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, SMALL_SYM_WIDTH))
  }
  drawSmallO(i: number, j: number, ii: number, jj: number) {
    let x = j * SUPERGRID_WIDTH + jj * GRID_WIDTH + INTERIOR_OFFSET +
        GRID_WIDTH / 2;
    let y = i * SUPERGRID_WIDTH + ii * GRID_WIDTH + INTERIOR_OFFSET +
        GRID_WIDTH / 2;
    this.svg.add(new Circle(
        x, y, GRID_WIDTH / 2 - MARKING_OFFSET, O_COLOR, SMALL_SYM_WIDTH));
  }
  addSmallLink(i: number, j: number, ii: number, jj: number, callback) {
    let x = j * SUPERGRID_WIDTH + jj * GRID_WIDTH + INTERIOR_OFFSET;
    let y = i * SUPERGRID_WIDTH + ii * GRID_WIDTH + INTERIOR_OFFSET;
    this.svg.add(new ClickableCaller(
        x, y, GRID_WIDTH, GRID_WIDTH, callback, LINK_COLOR, 0.1));
  }
  drawBigX(i: number, j: number) {
    let x = j * SUPERGRID_WIDTH + MARKING_OFFSET;
    let y = i * SUPERGRID_WIDTH + MARKING_OFFSET;
    this.svg.add(new Line(
        x, y, x + SUPERGRID_WIDTH - 2 * MARKING_OFFSET,
        y + SUPERGRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, BIG_SYM_WIDTH));
    this.svg.add(new Line(
        x + SUPERGRID_WIDTH - 2 * MARKING_OFFSET, y, x,
        y + SUPERGRID_WIDTH - 2 * MARKING_OFFSET, X_COLOR, BIG_SYM_WIDTH));
  }

  drawBigO(i: number, j: number) {
    let x = j * SUPERGRID_WIDTH + SUPERGRID_WIDTH / 2;
    let y = i * SUPERGRID_WIDTH + SUPERGRID_WIDTH / 2;
    this.svg.add(new Circle(
        x, y, SUPERGRID_WIDTH / 2 - MARKING_OFFSET, O_COLOR, BIG_SYM_WIDTH));
  }

  render(): string {
    return this.svg.render();
  }
}