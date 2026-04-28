package jack.client.mixin;

import jack.client.JackClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.hud.InGameHud;
import net.minecraft.client.render.RenderTickCounter;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(InGameHud.class)
public class MixinInGameHud {
    @Inject(method = "render", at = @At("TAIL"))
    private void onRender(DrawContext context, RenderTickCounter tickCounter, CallbackInfo ci) {
        if (JackClient.moduleManager != null) {
            // In 1.21, RenderTickCounter might use getLastFrameDuration() or getTickDelta(true)
            // If getTickDelta(boolean) is missing, we can use getLastFrameDuration() or just pass 1.0f for now
            JackClient.moduleManager.onRender(context, tickCounter.getLastFrameDuration());
        }
    }
}
