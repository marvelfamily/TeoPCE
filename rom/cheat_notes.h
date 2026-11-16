#ifndef TEOPCE_CHEAT_NOTES_H
#define TEOPCE_CHEAT_NOTES_H
/* Copies a whole note or returns zero; no banked pointer escapes.
 * Information is displayed inline; actionable advice requires confirmation. */
#define CHEAT_NOTE_ADVICE 1
#define CHEAT_NOTE_INFO 2
unsigned char cheat_note(unsigned long key,unsigned long id,unsigned long size,unsigned long crc,char *out,unsigned char cap);
/* Applies only to the exact catalog key, source group and target revision. */
unsigned char cheat_held(unsigned long key,unsigned long id,unsigned long size,unsigned long crc);
#endif
