package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;

public class ESP extends Module {
    public static ESP instance;

    public ESP() {
        super("ESP", Category.RENDER, 0);
        instance = this;
    }
}
