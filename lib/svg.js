var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var Renderable = /** @class */ (function () {
    function Renderable() {
    }
    Renderable.prototype.render = function () {
        return 'Must be subclassed!';
    };
    return Renderable;
}());
var SVG = /** @class */ (function () {
    function SVG(width, height) {
        this.width = width;
        this.height = height;
        this.elements = [];
    }
    SVG.prototype.add = function () {
        var elements = [];
        for (var _i = 0; _i < arguments.length; _i++) {
            elements[_i] = arguments[_i];
        }
        var _a;
        (_a = this.elements).push.apply(_a, elements);
    };
    SVG.prototype.render = function () {
        var strings = [
            "<svg viewBox=\"0 0 " + this.width + " " + this.height + "\" style=\"width: 100%; max-width: 100%; max-height: 100%; display: block;\">"
        ];
        this.elements.forEach(function (element) {
            strings.push(element.render());
        });
        strings.push('</svg>');
        return strings.join('\n');
    };
    return SVG;
}());
var Line = /** @class */ (function (_super) {
    __extends(Line, _super);
    function Line(x1, y1, x2, y2, color, width) {
        var _this = _super.call(this) || this;
        _this.x1 = x1;
        _this.x2 = x2;
        _this.y1 = y1;
        _this.y2 = y2;
        _this.color = color;
        _this.width = width;
        return _this;
    }
    Line.prototype.render = function () {
        return "<line x1=\"" + this.x1 + "\" y1=\"" + this.y1 + "\" x2=\"" + this.x2 + "\" y2=\"" + this.y2 + "\" style=\"stroke:" + this.color + "; stroke-width:" + this.width + "\" />";
    };
    return Line;
}(Renderable));
var ClickableCaller = /** @class */ (function (_super) {
    __extends(ClickableCaller, _super);
    function ClickableCaller(x, y, width, height, id, color, opacity) {
        var _this = _super.call(this) || this;
        _this.color = '#FFFFFF';
        _this.opacity = 0.1;
        _this.x = x;
        _this.y = y;
        _this.width = width;
        _this.height = height;
        _this.id = id;
        _this.opacity = opacity;
        _this.color = color;
        return _this;
    }
    ClickableCaller.prototype.render = function () {
        return " <rect x=\"" + this.x + "\" y=\"" + this.y + "\" width=\"" + this.width + "\" height=\"" + this.height + "\" opacity=\"" + this.opacity + "\" fill=\"" + this.color + "\" id=\"" + this.id + "\"/>";
    };
    return ClickableCaller;
}(Renderable));
var Circle = /** @class */ (function (_super) {
    __extends(Circle, _super);
    function Circle(x, y, radius, color, width) {
        var _this = _super.call(this) || this;
        _this.x = x;
        _this.y = y;
        _this.r = radius;
        _this.color = color;
        _this.width = width;
        return _this;
    }
    Circle.prototype.render = function () {
        return "<circle cx=\"" + this.x + "\" cy=\"" + this.y + "\" r=\"" + this.r + "\" style=\"stroke:" + this.color + "; stroke-width:" + this.width + "; fill: none\" />";
    };
    return Circle;
}(Renderable));
