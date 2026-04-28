package jack.client.module.modules.player;

import jack.client.mixin.PlayerMoveC2SPacketAccessor;
import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.network.packet.Packet;
import net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket;

public class NoFall extends Module {

    public NoFall() {
        super("NoFall", Category.PLAYER, 0);
    }

    @Override
    public boolean onSendPacket(Packet<?> packet) {
        if (mc.player == null) return false;

        // Intercept outgoing movement packets
        if (packet instanceof PlayerMoveC2SPacket movePacket) {
            // If we are falling fast enough to take damage
            if (mc.player.fallDistance > 2.0f) {
                // Use the accessor to modify the packet before it goes to the server
                ((PlayerMoveC2SPacketAccessor) movePacket).setOnGround(true);
            }
        }

        return false; // Don't cancel, just modify
    }
}
