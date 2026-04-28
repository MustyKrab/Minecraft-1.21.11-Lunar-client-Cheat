package jack.client.mixin;

import net.minecraft.client.ClientBrandRetriever;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(ClientBrandRetriever.class)
public class MixinClientBrandRetriever {

    @Inject(method = "getClientModName", at = @At("HEAD"), cancellable = true)
    private static void spoofClientBrand(CallbackInfoReturnable<String> cir) {
        // Spoof the client brand to appear as vanilla to the server.
        // This is the first step in bypassing server-side anticheats that check for "fabric".
        cir.setReturnValue("vanilla");
    }
}
