package jack.client.module.modules.player;

import jack.client.mixin.PlayerMoveC2SPacketAccessor;
import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.network.packet.Packet;
import net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket;

public class NoFall extends Module {

    private boolean shouldCatch = false;

    public NoFall() {
        super("NoFall", Category.PLAYER, 0);
    }

    @Override
    public void onTick() {
        if (mc.player == null) return;
        
        // Instead of spamming onGround, we only send it when we are about to hit the ground.
        // This bypasses most basic anticheats that check for mid-air onGround spooffing.
        if (mc.player.fallDistance > 2.5f) {
            // Check if we are about to hit the ground (1 block below)
            if (!mc.world.isSpaceEmpty(mc.player.getBoundingBox().offset(0, -1.0, 0)) || 
                !mc.world.isSpaceEmpty(mc.player.getBoundingBox().offset(0, -0.5, 0))) {
                shouldCatch = true;
            }
        } else {
            shouldCatch = false;
        }
    }

    @Override
    public boolean onSendPacket(Packet<?> packet) {
        if (mc.player == null) return false;

        if (packet instanceof PlayerMoveC2SPacket movePacket) {
            if (shouldCatch) {
                ((PlayerMoveC2SPacketAccessor) movePacket).setOnGround(true);
                shouldCatch = false; // Reset after catching
            }
        }

        return false;
    }
}
