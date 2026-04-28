package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;

public class GlowESP extends Module {
    public static GlowESP instance;

    public GlowESP() {
        super("GlowESP", Category.RENDER, 0);
        instance = this;
    }
}
