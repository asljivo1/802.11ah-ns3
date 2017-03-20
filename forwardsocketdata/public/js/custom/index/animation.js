var __extends = (this && this.__extends) || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
};
var Animation = (function () {
    function Animation() {
        this.time = 0;
        this.color = new Color(0, 0, 0, 1, 0);
    }
    Animation.prototype.update = function (dt) {
        this.time += dt;
    };
    return Animation;
})();
var BroadcastAnimation = (function (_super) {
    __extends(BroadcastAnimation, _super);
    function BroadcastAnimation(x, y) {
        _super.call(this);
        this.x = x;
        this.y = y;
        this.max_radius = 50;
        this.max_time = 1000;
        this.color = new Color(255, 0, 0);
    }
    BroadcastAnimation.prototype.draw = function (canvas, ctx, area) {
        var radius = this.time / this.max_time * this.max_radius;
        this.color.alpha = 1 - this.time / this.max_time;
        ctx.strokeStyle = this.color.toString();
        ctx.beginPath();
        ctx.arc(this.x * (canvas.width / area), this.y * (canvas.width / area), radius, 0, Math.PI * 2, false);
        ctx.stroke();
    };
    BroadcastAnimation.prototype.isFinished = function () {
        return this.time >= this.max_time;
    };
    return BroadcastAnimation;
})(Animation);
var ReceivedAnimation = (function (_super) {
    __extends(ReceivedAnimation, _super);
    function ReceivedAnimation(x, y) {
        _super.call(this);
        this.x = x;
        this.y = y;
        this.max_radius = 10;
        this.max_time = 1000;
        this.color = new Color(0, 255, 0);
    }
    ReceivedAnimation.prototype.draw = function (canvas, ctx, area) {
        var radius = (1 - this.time / this.max_time) * this.max_radius;
        this.color.alpha = 1 - this.time / this.max_time;
        ctx.strokeStyle = this.color.toString();
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.arc(this.x * (canvas.width / area), this.y * (canvas.width / area), radius, 0, Math.PI * 2, false);
        ctx.stroke();
    };
    ReceivedAnimation.prototype.isFinished = function () {
        return this.time >= this.max_time;
    };
    return ReceivedAnimation;
})(Animation);
var AssociatedAnimation = (function (_super) {
    __extends(AssociatedAnimation, _super);
    function AssociatedAnimation(x, y) {
        _super.call(this);
        this.x = x;
        this.y = y;
        this.max_time = 3000;
    }
    AssociatedAnimation.prototype.draw = function (canvas, ctx, area) {
        var offset = this.time / this.max_time * Math.PI * 2;
        this.color.alpha = 1 - this.time / this.max_time;
        ctx.strokeStyle = this.color.toString();
        ctx.beginPath();
        ctx.setLineDash(([10, 2]));
        ctx.lineWidth = 3;
        ctx.arc(this.x * (canvas.width / area), this.y * (canvas.width / area), 10, offset, offset + Math.PI * 2, false);
        ctx.stroke();
        ctx.setLineDash([]);
        ctx.lineWidth = 1;
    };
    AssociatedAnimation.prototype.isFinished = function () {
        return this.time >= this.max_time;
    };
    return AssociatedAnimation;
})(Animation);
//# sourceMappingURL=animation.js.map