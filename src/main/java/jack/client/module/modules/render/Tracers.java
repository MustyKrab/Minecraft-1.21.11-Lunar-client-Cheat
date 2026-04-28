package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.LivingEntity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;

public class Tracers extends Module {
    public Tracers() {
        super("Tracers", Category.RENDER, 0);
    }

    @Override
    public void onRender(DrawContext context, float tickDelta) {
        if (mc.world == null || mc.player == null) return;

        int width = mc.getWindow().getScaledWidth();
        int height = mc.getWindow().getScaledHeight();
        
        // Use player's camera pos instead of gameRenderer.getCamera().getPos() to avoid mapping issues
        Vec3d cameraPos = mc.player.getCameraPosVec(tickDelta);

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                Vec3d entityPos = entity.getLerpedPos(tickDelta);
                
                Vec3d diff = entityPos.subtract(cameraPos);
                Vec3d look = mc.player.getRotationVec(tickDelta);
                
                if (diff.normalize().dotProduct(look) > 0) {
                    double yawDiff = Math.toDegrees(Math.atan2(diff.z, diff.x)) - 90 - mc.player.getYaw(tickDelta);
                    double pitchDiff = Math.toDegrees(Math.atan2(-diff.y, Math.sqrt(diff.x*diff.x + diff.z*diff.z))) - mc.player.getPitch(tickDelta);
                    
                    while (yawDiff <= -180) yawDiff += 360;
                    while (yawDiff > 180) yawDiff -= 360;
                    
                    int screenX = width / 2 + (int)(yawDiff * 10);
                    int screenY = height / 2 + (int)(pitchDiff * 10);
                    
                    context.fill(screenX - 1, screenY - 1, screenX + 1, screenY + 1, 0xFFFF0000);
                }
            }
        }
    }
}
