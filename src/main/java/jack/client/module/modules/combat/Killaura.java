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
import net.minecraft.util.math.MathHelper;
import net.minecraft.util.math.Vec3d;

import java.util.Comparator;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

public class Killaura extends Module {

    private float serverYaw;
    private float serverPitch;
    private Entity target;
    private int hitDelay = 0;

    public Killaura() {
        super("Killaura", Category.COMBAT, 0);
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
            // Anticheats flag perfect floating point rotations.
            float f = (float) (mc.options.getMouseSensitivity().getValue() * 0.6F + 0.2F);
            float gcd = f * f * f * 1.2F;
            
            rotations[0] -= rotations[0] % gcd;
            rotations[1] -= rotations[1] % gcd;

            serverYaw = rotations[0];
            serverPitch = rotations[1];

            // 3. Attack logic (respecting cooldowns for 1.21.11 combat)
            if (mc.player.getAttackCooldownProgress(0.5f) >= 1.0f) {
                // In a true ghost client, we might want to raytrace here to ensure we are actually looking at the hitbox
                // before sending the attack packet, but for a blatant aura, we just attack.
                mc.interactionManager.attackEntity(mc.player, target);
                mc.player.swingHand(Hand.MAIN_HAND);
            }
        }
    }

    @Override
    public boolean onSendPacket(Packet<?> packet) {
        if (mc.player == null || target == null) return false;

        // 4. Silent Rotation (Spoofing)
        // We modify the outgoing movement packets to send our calculated yaw/pitch to the server
        // while our actual client camera remains unaffected.
        if (packet instanceof PlayerMoveC2SPacket movePacket) {
            if (movePacket.changesLook()) {
                ((PlayerMoveC2SPacketAccessor) movePacket).setYaw(serverYaw);
                ((PlayerMoveC2SPacketAccessor) movePacket).setPitch(serverPitch);
            } else {
                // If the packet doesn't have look data, we might need to force send a Look packet
                // or upgrade it to a PositionAndLook packet depending on the strictness of the AC.
            }
        }
        return false;
    }

    private Entity findTarget() {
        List<Entity> entities = StreamSupport.stream(mc.world.getEntities().spliterator(), false)
                .filter(e -> e instanceof LivingEntity)
                .filter(e -> e != mc.player)
                .filter(e -> mc.player.distanceTo(e) <= 4.5f) // Range check
                .filter(e -> ((LivingEntity) e).getHealth() > 0) // Alive check
                .sorted(Comparator.comparingDouble(e -> mc.player.distanceTo(e))) // Sort by distance
                .collect(Collectors.toList());

        return entities.isEmpty() ? null : entities.get(0);
    }

    private float[] getRotations(Entity target) {
        Vec3d playerPos = mc.player.getEyePos();
        // Aim at the center of the target's bounding box
        Vec3d targetPos = target.getPos().add(0, target.getHeight() / 2.0, 0);

        double diffX = targetPos.x - playerPos.x;
        double diffY = targetPos.y - playerPos.y;
        double diffZ = targetPos.z - playerPos.z;

        double dist = Math.sqrt(diffX * diffX + diffZ * diffZ);

        float yaw = (float) (Math.atan2(diffZ, diffX) * 180.0D / Math.PI) - 90.0F;
        float pitch = (float) -(Math.atan2(diffY, dist) * 180.0D / Math.PI);

        // Add slight randomization to bypass heuristic checks
        yaw += (Math.random() - 0.5) * 2.0;
        pitch += (Math.random() - 0.5) * 2.0;

        return new float[]{
                mc.player.getYaw() + MathHelper.wrapDegrees(yaw - mc.player.getYaw()),
                mc.player.getPitch() + MathHelper.wrapDegrees(pitch - mc.player.getPitch())
        };
    }
}
