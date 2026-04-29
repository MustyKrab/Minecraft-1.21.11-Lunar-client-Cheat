package musty.client.module.modules.combat;

import musty.client.module.Category;
import musty.client.module.Module;
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

        if (packet instanceof EntityVelocityUpdateS2CPacket velocityPacket) {
            // If getId() and id() both fail, Yarn mappings typically use getEntityId()
            if (velocityPacket.getEntityId() == mc.player.getId()) {
                return true;
            }
        }

        if (packet instanceof ExplosionS2CPacket) {
            return true;
        }

        return false;
    }
}
