import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace inet;

// Enum for Node Roles
enum HCCRole {
    UNDECIDED = 0;
    CLUSTER_HEAD = 1;
    CLUSTER_MEMBER = 2;
}

// The Beacon Message extending INET's FieldsChunk (standard for INET 4.5)
class HCCPacket extends FieldsChunk {
    int srcId;              // The ID of the sender
    int degree;             // The sender's connectivity (number of neighbors)
    int role;               // The sender's current state (enum HCCRole)
    int clusterHeadId;      // Who the sender thinks is the CH (if applicable)
}