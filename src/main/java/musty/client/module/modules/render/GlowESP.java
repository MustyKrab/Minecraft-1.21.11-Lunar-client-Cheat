package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;

public class GlowESP extends Module {
    public static GlowESP instance;

    public GlowESP() {
        super("GlowESP", Category.RENDER, 0);
        instance = this;
    }
}
