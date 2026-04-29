package musty.client.module.modules.player;

import musty.client.mixin.PlayerMoveC2SPacketAccessor;
import musty.client.module.Category;
import musty.client.module.Module;
import net.minecraft.network.packet.Packet;
import net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket;

public class NoFall extends Module {
    public NoFall() {
        super("NoFall", Category.PLAYER, 0);
    }

    @Override
    public boolean onSendPacket(Packet<?> packet) {
        if (mc.player == null) return false;

        if (packet instanceof PlayerMoveC2SPacket) {
            if (mc.player.fallDistance > 2.0f) {
                ((PlayerMoveC2SPacketAccessor) packet).setOnGround(true);
            }
        }

        return false;
    }
}
