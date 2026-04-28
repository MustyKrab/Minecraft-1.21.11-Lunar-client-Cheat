package jack.client.module.modules.combat;

import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.network.packet.Packet;
import net.minecraft.network.packet.s2c.play.EntityVelocityUpdateS2CPacket;
import net.minecraft.network.packet.s2c.play.ExplosionS2CPacket;

public class Velocity extends Module {

    public Velocity() {
        super("Velocity", Category.COMBAT, 0);
    }

    @Override
    public boolean onReceivePacket(Packet<?> packet) {
        if (mc.player == null) return false;

        // Cancel standard entity velocity updates (Knockback from hits)
        if (packet instanceof EntityVelocityUpdateS2CPacket velocityPacket) {
            if (velocityPacket.getEntityId() == mc.player.getId()) {
                return true; // Cancel packet
            }
        }

        // Cancel explosion knockback
        if (packet instanceof ExplosionS2CPacket) {
            return true; // Cancel packet
        }

        return false;
    }
}
