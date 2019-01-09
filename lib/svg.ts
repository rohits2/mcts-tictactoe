class Renderable {
  render(): string {
    return 'Must be subclassed!';
  }
}
class SVG {
  width: number;
  height: number;
  elements: Array<Renderable>;
  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.elements = [];
  }
  add(...elements) {
    this.elements.push(...elements);
  }
  render(): string {
    let strings = [
      `<svg viewBox="0 0 ${this.width} ${this.height}" style="width: 100%; max-width: 100%; max-height: 100%; display: block;">`
    ];
    this.elements.forEach(element => {
      strings.push(element.render());
    });
    strings.push('</svg>')
    return strings.join('\n')
  }
}

class Line extends Renderable {
  x1: Number;
  y1: Number;
  x2: Number;
  y2: Number;
  color: string;
  width: number;
  constructor(
      x1: Number, y1: Number, x2: Number, y2: Number, color: string,
      width: number) {
    super();
    this.x1 = x1;
    this.x2 = x2;
    this.y1 = y1;
    this.y2 = y2;
    this.color = color;
    this.width = width;
  }
  render(): string {
    return `<line x1="${this.x1}" y1="${this.y1}" x2="${this.x2}" y2="${
        this.y2}" style="stroke:${this.color}; stroke-width:${this.width}" />`
  }
}

class ClickableCaller extends Renderable {
  x: number;
  y: number;
  width: number;
  height: number;
  id: string;
  color: string = '#FFFFFF';
  opacity: number = 0.1;
  constructor(
      x: number, y: number, width: number, height: number, id: string,
      color: string, opacity) {
    super();
    this.x = x;
    this.y = y;
    this.width = width;
    this.height = height;
    this.id = id;
    this.opacity = opacity;
    this.color = color;
  }

  render(): string {
    return ` <rect x="${this.x}" y="${this.y}" width="${this.width}" height="${
        this.height}" opacity="${this.opacity}" fill="${this.color}" id="${
        this.id}"/>`
  }
}

class Circle extends Renderable {
  x: Number;
  y: Number;
  r: Number;
  color: string;
  width: number;
  constructor(
      x: Number, y: Number, radius: Number, color: string, width: number) {
    super();
    this.x = x;
    this.y = y;
    this.r = radius;
    this.color = color;
    this.width = width;
  }
  render(): string {
    return `<circle cx="${this.x}" cy="${this.y}" r="${this.r}" style="stroke:${
        this.color}; stroke-width:${this.width}; fill: none" />`
  }
}