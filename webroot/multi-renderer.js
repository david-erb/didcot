export class MultiRenderer {
    constructor(renderers = []) {
        this.renderers = renderers;
    }

    render(model) {
        for (const renderer of this.renderers) {
            renderer.render(model);
        }
    }
}
