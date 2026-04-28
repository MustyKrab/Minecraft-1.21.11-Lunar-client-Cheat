package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;

public class ESP extends Module {
    public static ESP instance;

    public ESP() {
        super("ESP", Category.RENDER, 0);
        instance = this;
    }
}
