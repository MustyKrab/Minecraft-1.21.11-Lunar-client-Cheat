package jack.client.mixin;

import jack.client.module.modules.render.GlowESP;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(Entity.class)
public class MixinEntity {
    @Inject(method = "isGlowing", at = @At("HEAD"), cancellable = true)
    private void onIsGlowing(CallbackInfoReturnable<Boolean> cir) {
        if (GlowESP.instance != null && GlowESP.instance.isEnabled()) {
            if ((Object) this instanceof PlayerEntity) {
                cir.setReturnValue(true);
            }
        }
    }
}
