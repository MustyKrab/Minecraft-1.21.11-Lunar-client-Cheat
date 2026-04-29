package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Vector3f;

public class SkeletonESP extends Module {
    public SkeletonESP() {
        super("SkeletonESP", Category.RENDER, 0);
    }

    @Override
    public void onRender(DrawContext context, float tickDelta) {
        if (mc.world == null || mc.player == null) return;

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                PlayerEntity player = (PlayerEntity) entity;
                Vec3d pos = player.getLerpedPos(tickDelta);
                
                // Approximate bone positions based on body yaw
                float bodyYaw = (float) Math.toRadians(player.bodyYaw);
                float cos = (float) Math.cos(bodyYaw);
                float sin = (float) Math.sin(bodyYaw);
                
                Vec3d rightOffset = new Vec3d(-cos * 0.3, 0, -sin * 0.3);
                Vec3d leftOffset = new Vec3d(cos * 0.3, 0, sin * 0.3);
                
                Vec3d head = pos.add(0, player.getHeight(), 0);
                Vec3d neck = pos.add(0, player.getHeight() - 0.4, 0);
                Vec3d pelvis = pos.add(0, player.getHeight() - 1.1, 0);
                
                Vec3d rightShoulder = neck.add(rightOffset);
                Vec3d leftShoulder = neck.add(leftOffset);
                Vec3d rightHand = rightShoulder.add(0, -0.6, 0);
                Vec3d leftHand = leftShoulder.add(0, -0.6, 0);
                
                Vec3d rightHip = pelvis.add(rightOffset.multiply(0.5));
                Vec3d leftHip = pelvis.add(leftOffset.multiply(0.5));
                Vec3d rightFoot = pos.add(rightOffset.multiply(0.5));
                Vec3d leftFoot = pos.add(leftOffset.multiply(0.5));

                Vector3f pHead = project2D(head, tickDelta);
                Vector3f pNeck = project2D(neck, tickDelta);
                Vector3f pPelvis = project2D(pelvis, tickDelta);
                Vector3f pRS = project2D(rightShoulder, tickDelta);
                Vector3f pLS = project2D(leftShoulder, tickDelta);
                Vector3f pRH = project2D(rightHand, tickDelta);
                Vector3f pLH = project2D(leftHand, tickDelta);
                Vector3f pRHip = project2D(rightHip, tickDelta);
                Vector3f pLHip = project2D(leftHip, tickDelta);
                Vector3f pRFoot = project2D(rightFoot, tickDelta);
                Vector3f pLFoot = project2D(leftFoot, tickDelta);

                if (pHead != null && pNeck != null && pPelvis != null && pRS != null && pLS != null && pRH != null && pLH != null && pRHip != null && pLHip != null && pRFoot != null && pLFoot != null) {
                    int color = 0xFFFFFFFF;
                    
                    // Spine
                    drawLine(context, (int)pHead.x, (int)pHead.y, (int)pNeck.x, (int)pNeck.y, color);
                    drawLine(context, (int)pNeck.x, (int)pNeck.y, (int)pPelvis.x, (int)pPelvis.y, color);
                    
                    // Arms
                    drawLine(context, (int)pNeck.x, (int)pNeck.y, (int)pRS.x, (int)pRS.y, color);
                    drawLine(context, (int)pRS.x, (int)pRS.y, (int)pRH.x, (int)pRH.y, color);
                    
                    drawLine(context, (int)pNeck.x, (int)pNeck.y, (int)pLS.x, (int)pLS.y, color);
                    drawLine(context, (int)pLS.x, (int)pLS.y, (int)pLH.x, (int)pLH.y, color);
                    
                    // Legs
                    drawLine(context, (int)pPelvis.x, (int)pPelvis.y, (int)pRHip.x, (int)pRHip.y, color);
                    drawLine(context, (int)pRHip.x, (int)pRHip.y, (int)pRFoot.x, (int)pRFoot.y, color);
                    
                    drawLine(context, (int)pPelvis.x, (int)pPelvis.y, (int)pLHip.x, (int)pLHip.y, color);
                    drawLine(context, (int)pLHip.x, (int)pLHip.y, (int)pLFoot.x, (int)pLFoot.y, color);
                }
            }
        }
    }
