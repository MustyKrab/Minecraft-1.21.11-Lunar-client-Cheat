package jack.client.module.modules.combat;

import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.network.packet.Packet;
import net.minecraft.network.packet.s2c.play.EntityVelocityUpdateS2CPacket;
import net.minecraft.network.packet.s2c.play.ExplosionS2CPacket;
import net.minecraft.util.math.Vec3d;

public class Velocity extends Module {

    public Velocity() {
        super("Velocity", Category.COMBAT, 0);
    }

    @Override
    public boolean onReceivePacket(Packet<?> packet) {
        if (mc.player == null) return false;

        if (packet instanceof EntityVelocityUpdateS2CPacket velocityPacket) {
            if (velocityPacket.getEntityId() == mc.player.getId()) {
                // Instead of cancelling, we scale the velocity down.
                // This bypasses AntiKnockback checks because we still take some knockback.
                double horizontalScale = 0.8; // 80% horizontal knockback
                double verticalScale = 1.0;  // 100% vertical knockback
                
                mc.player.setVelocity(new Vec3d(
                    (velocityPacket.getVelocityX() / 8000.0D) * horizontalScale,
                    (velocityPacket.getVelocityY() / 8000.0D) * verticalScale,
                    (velocityPacket.getVelocityZ() / 8000.0D) * horizontalScale
                ));
                return true; // Cancel the original packet since we applied it manually
            }
        }

        if (packet instanceof ExplosionS2CPacket explosionPacket) {
            double horizontalScale = 0.8;
            double verticalScale = 1.0;
            
            mc.player.setVelocity(new Vec3d(
                mc.player.getVelocity().x + (explosionPacket.getPlayerVelocityX() * horizontalScale),
                mc.player.getVelocity().y + (explosionPacket.getPlayerVelocityY() * verticalScale),
                mc.player.getVelocity().z + (explosionPacket.getPlayerVelocityZ() * horizontalScale)
            ));
            return true;
        }

        return false;
    }
}
