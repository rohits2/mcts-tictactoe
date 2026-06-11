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
      `<svg viewBox="0 0 ${this.width} ${this.height}" style="width: 100%; max-width: 100%; max-height: 100%; display: block;">`,
      // Radial gradient used by the cell-link hover state: transparent in the
      // centre, green toward the edges, so a hovered cell shows a faint inward
      // glow (see rect.cell-link:hover in material.css).
      `<defs><radialGradient id="cell-hover-glow" cx="50%" cy="50%" r="55%">` +
      `<stop offset="38%" stop-color="#33ff99" stop-opacity="0"/>` +
      `<stop offset="100%" stop-color="#33ff99" stop-opacity="0.6"/>` +
      `</radialGradient></defs>`
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
  className: string;
  constructor(
      x1: Number, y1: Number, x2: Number, y2: Number, color: string,
      width: number, className: string = '') {
    super();
    this.x1 = x1;
    this.x2 = x2;
    this.y1 = y1;
    this.y2 = y2;
    this.color = color;
    this.width = width;
    this.className = className;
  }
  render(): string {
    return `<line x1="${this.x1}" y1="${this.y1}" x2="${this.x2}" y2="${
        this.y2}" class="${this.className}" style="stroke:${this.color}; stroke-width:${this.width}" />`
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
    return ` <rect class="cell-link" x="${this.x}" y="${this.y}" width="${this.width}" height="${
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
  className: string;
  constructor(
      x: Number, y: Number, radius: Number, color: string, width: number,
      className: string = '') {
    super();
    this.x = x;
    this.y = y;
    this.r = radius;
    this.color = color;
    this.width = width;
    this.className = className;
  }
  render(): string {
    return `<circle cx="${this.x}" cy="${this.y}" r="${this.r}" class="${
        this.className}" style="stroke:${this.color}; stroke-width:${this.width}; fill: none" />`
  }
}

class Label extends Renderable {
  x: number;
  y: number;
  text: string;
  color: string;
  size: number;
  constructor(
      x: number, y: number, text: string, color: string, size: number) {
    super();
    this.x = x;
    this.y = y;
    this.text = text;
    this.color = color;
    this.size = size;
  }
  render(): string {
    // pointer-events:none so the label never intercepts a click on the valid-move
    // rect underneath it.
    return `<text x="${this.x}" y="${this.y}" font-size="${this.size}" fill="${
        this.color}" text-anchor="middle" dominant-baseline="central" style="font-family: monospace; font-weight: bold; pointer-events: none;">${
        this.text}</text>`
  }
}