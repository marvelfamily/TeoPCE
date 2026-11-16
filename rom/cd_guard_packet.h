#ifndef TEOPCE_CD_GUARD_PACKET_H
#define TEOPCE_CD_GUARD_PACKET_H
/* Addresses are normalized HuC6280 CPU addresses, in native row order. */
typedef struct {
    unsigned char rows[48], signature[8], vectors[6];
    unsigned char siglo, sighi, count, bha;
} CdGuardPacket;
/* init copies signature and vectors. bha: 0 none, 1 BHA P1, 2 BHA P2. */
void cd_packet_init(CdGuardPacket *s, unsigned short signature_start,
                    const unsigned char *signature, const unsigned char *vectors,
                    unsigned char bha);
/* add and validate return zero on success. Invalid rows do not alter state. */
unsigned char cd_packet_add(CdGuardPacket *s, unsigned short address, unsigned char value);
unsigned char cd_packet_validate(const CdGuardPacket *s);
/* Guard size is zero on validation failure. Empty selections remain disabled.
 * byte returns zero for out-of-range offsets. */
unsigned char cd_packet_size(const CdGuardPacket *s, unsigned char guarded);
unsigned char cd_packet_byte(const CdGuardPacket *s, unsigned char guarded, unsigned char offset);
#endif
