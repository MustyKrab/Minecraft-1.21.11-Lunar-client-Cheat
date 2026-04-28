package jack.client.module.modules.combat;

import jack.client.mixin.PlayerMoveC2SPacketAccessor;
import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.entity.Entity;
import net.minecraft.entity.LivingEntity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.network.packet.Packet;
import net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket;
import net.minecraft.util.Hand;
import net.minecraft.util.hit.HitResult;
import net.minecraft.util.hit.BlockHitResult;
import net.minecraft.util.math.MathHelper;
import net.minecraft.util.math.Vec3d;
import net.minecraft.world.RaycastContext;

import java.util.Comparator;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

public class Killaura extends Module {

    private float serverYaw;
    private float serverPitch;
    private Entity target;
    private float reach = 4.5f;

    public Killaura() {
        super("Killaura", Category.COMBAT, 0);
    }

    public float getReach() {
        return reach;
    }

    public void setReach(float reach) {
        this.reach = reach;
    }

    @Override
    public void onTick() {
        if (mc.player == null || mc.world == null) return;

        // 1. Find the best target
        target = findTarget();

        if (target != null) {
            // 2. Calculate rotations
            float[] rotations = getRotations(target);
            
            // Apply GCD fix (Greatest Common Divisor) to simulate real mouse movements
            float f = (float) (mc.options.getMouseSensitivity().getValue() * 0.6F + 0.2F);
            float gcd = f * f * f * 1.2F;
            
            rotations[0] -= rotations[0] % gcd;
            rotations[1] -= rotations[1] % gcd;

            serverYaw = rotations[0];
            serverPitch = rotations[1];

            // 3. Attack logic (respecting cooldowns for 1.21.11 combat)
            if (mc.player.getAttackCooldownProgress(0.5f) >= 1.0f) {
                // Raytrace check to prevent hitting through walls
                if (isVisible(target)) {
                    mc.interactionManager.attackEntity(mc.player, target);
                    mc.player.swingHand(Hand.MAIN_HAND);
                }
            }
        }
    }

    @Override
    public boolean onSendPacket(Packet<?> packet) {
        if (mc.player == null || target == null) return false;

        // 4. Silent Rotation (Spoofing)
        if (packet instanceof PlayerMoveC2SPacket movePacket) {
            if (movePacket.changesLook()) {
                ((PlayerMoveC2SPacketAccessor) movePacket).setYaw(serverYaw);
                ((PlayerMoveC2SPacketAccessor) movePacket).setPitch(serverPitch);
            }
        }
        return false;
    }

    private boolean isVisible(Entity entity) {
        Vec3d playerEyes = mc.player.getEyePos();
        Vec3d entityCenter = entity.getBoundingBox().getCenter();
        
        RaycastContext context = new RaycastContext(
            playerEyes, 
            entityCenter, 
            RaycastContext.ShapeType.COLLIDER, 
            RaycastContext.FluidHandling.NONE, 
            mc.player
        );
        
        BlockHitResult result = mc.world.raycast(context);
        return result.getType() == HitResult.Type.MISS;
    }

    private Entity findTarget() {
        List<Entity> entities = StreamSupport.stream(mc.world.getEntities().spliterator(), false)
                .filter(e -> e instanceof LivingEntity)
                .filter(e -> e != mc.player)
                .filter(e -> mc.player.distanceTo(e) <= reach)
                .filter(e -> ((LivingEntity) e).getHealth() > 0)
                .sorted(Comparator.comparingDouble(e -> mc.player.distanceTo(e)))
                .collect(Collectors.toList());

        return entities.isEmpty() ? null : entities.get(0);
    }

    private float[] getRotations(Entity target) {
        Vec3d playerPos = mc.player.getEyePos();
        // Aim at the center of the target's bounding box
        Vec3d targetPos = target.getBoundingBox().getCenter();

        double diffX = targetPos.x - playerPos.x;
        double diffY = targetPos.y - playerPos.y;
        double diffZ = targetPos.z - playerPos.z;

        double dist = Math.sqrt(diffX * diffX + diffZ * diffZ);

        float yaw = (float) (Math.atan2(diffZ, diffX) * 180.0D / Math.PI) - 90.0F;
        float pitch = (float) (-Math.atan2(diffY, dist) * 180.0D / Math.PI);

        return new float[]{
                mc.player.getYaw() + MathHelper.wrapDegrees(yaw - mc.player.getYaw()),
                mc.player.getPitch() + MathHelper.wrapDegrees(pitch - mc.player.getPitch())
        };
    }
}
