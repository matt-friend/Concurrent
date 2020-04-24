
.global sem_post
.global sem_wait

sem_post: ldrex r1 , [ r0 ]          @ s' = MEM[ &s ]
          add r1 , r1 , #1           @ s' = s' + 1
          strex r2 , r1 , [ r0 ]     @ r <= MEM[ &s ] = s'
          cmp r2 , #0                @ r ?= 0
          bne sem_post               @ if r != 0, retry
          dmb                        @ memory barrier
          bx lr                      @ return

sem_wait: ldrex r1 , [ r0 ]          @ s' = MEM[ &s ]
          cmp r1 , #0                @ s' ?= 0
          beq sem_wait               @ if s' == 0, retry
          sub r1 , r1 , #1           @ s' = s' - 1
          strex r2 , r1 , [ r0 ]     @ r <= MEM[ &s ] = s'
          cmp r2 , #0                @ r ?= 0
          bne sem_wait               @ if r != 0, retry
          dmb                        @ memory barrier
          bx lr                      @ return
