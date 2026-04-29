package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Vector3f;

public class Tracers extends Module {
    public Tracers() {
        super("Tracers", Category.RENDER, 0);
    }

    @Override
    public void onRender(DrawContext context, float tickDelta) {
        if (mc.world == null || mc.player == null) return;

        int width = mc.getWindow().getScaledWidth();
        int height = mc.getWindow().getScaledHeight();
        int startX = width / 2;
        int startY = height;

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                Vec3d feetPos = entity.getLerpedPos(tickDelta);
                Vector3f projFeet = project2D(feetPos, tickDelta);

                if (projFeet != null) {
                    drawLine(context, startX, startY, (int)projFeet.x, (int)projFeet.y, 0xFFFF0000);
                }
            }
        }
    }
}
