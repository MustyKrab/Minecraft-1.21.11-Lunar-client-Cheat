package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderContext;
import net.minecraft.client.render.RenderLayer;
import net.minecraft.client.render.VertexConsumer;
import net.minecraft.client.render.VertexConsumerProvider;
import net.minecraft.client.render.VertexRendering;
import net.minecraft.client.util.math.MatrixStack;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Box;
import net.minecraft.util.math.Vec3d;
import net.minecraft.util.shape.VoxelShapes;
import org.joml.Matrix4f;

public class ESP extends Module {

    public ESP() {
        super("ESP", Category.RENDER, 0);
    }

    @Override
    protected void onEnable() {
        // Registering the render event. In 1.21.11, AFTER_ENTITIES is a good place for ESP.
        WorldRenderEvents.AFTER_ENTITIES.register(this::onRenderWorld);
    }

    @Override
    protected void onDisable() {
        // Note: Fabric events don't have a simple unregister. 
        // We handle disabling by checking isEnabled() inside the event callback.
    }
    
    private void onRenderWorld(WorldRenderContext context) {
        if (!isEnabled() || mc.world == null || mc.player == null) return;

        MatrixStack matrices = context.matrixStack();
        VertexConsumerProvider vertexConsumers = context.consumers();
        Vec3d cameraPos = context.camera().getPos();

        for (Entity entity : mc.world.getEntities()) {
            // Skip ourselves and non-players for now
            if (entity == mc.player || !(entity instanceof PlayerEntity)) continue;

            // Calculate position relative to camera
            double x = entity.lerpX(context.tickCounter().getTickDelta(true)) - cameraPos.x;
            double y = entity.lerpY(context.tickCounter().getTickDelta(true)) - cameraPos.y;
            double z = entity.lerpZ(context.tickCounter().getTickDelta(true)) - cameraPos.z;

            Box box = entity.getBoundingBox().offset(-entity.getX(), -entity.getY(), -entity.getZ());

            // Draw the 3D Outline Box
            // ARGB format: 0xAARRGGBB. Let's use a solid red box: 0xFFFF0000
            int color = 0xFFFF0000; 
            
            matrices.push();
            matrices.translate(x, y, z);

            // In 1.21.11, VertexRendering.drawOutline is the standard way to draw voxel shape outlines
            VertexConsumer consumer = vertexConsumers.getBuffer(RenderLayer.getLines());
            VertexRendering.drawOutline(
                matrices, 
                consumer, 
                VoxelShapes.cuboid(box), 
                0, 0, 0, 
                color, 
                2.0f // Line width
            );

            matrices.pop();
            
            // Note: To draw text (Nametags) or 2D health bars, you would typically use 
            // mc.textRenderer and project the 3D coordinates to 2D screen space, 
            // or draw them as floating text in the 3D world using the matrix stack.
        }
    }
}
