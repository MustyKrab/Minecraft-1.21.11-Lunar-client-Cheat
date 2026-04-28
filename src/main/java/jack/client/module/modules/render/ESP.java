package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderContext;

public class ESP extends Module {

    public ESP() {
        super("ESP", Category.RENDER, 0); // 0 = no keybind by default
    }

    @Override
    protected void onEnable() {
        // We will register our render event here or handle it in a mixin
    }

    @Override
    protected void onDisable() {
        // Unregister
    }
    
    // This will be called by WorldRenderEvents or a Mixin
    public void onRenderWorld(WorldRenderContext context) {
        if (!isEnabled() || mc.world == null || mc.player == null) return;
        
        // TODO: Port C++ WorldToScreen and drawing logic here
        // We will use context.matrixStack() and VertexConsumers
    }
}
