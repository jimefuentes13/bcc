/* Miscellaneous system calls.				Author: Kees J. Bot
 *								31 Mar 2000
 * The entry points into this file are:
 *   do_reboot: kill all processes, then reboot system
 *   do_procstat: request process status  (Jorrit N. Herder)
 *   do_getsysinfo: request copy of PM data structure  (Jorrit N. Herder)
 *   do_getprocnr: lookup process slot number  (Jorrit N. Herder)
 *   do_getepinfo: get the pid/uid/gid of a process given its endpoint
 *   do_getsetpriority: get/set process priority
 *   do_svrctl: process manager control
 */

#define brk _brk


/*?????????????????????????????????????????????????????????????????????????????????????*/
/*?????????????????????????????????????????????????????????????????????????????????????*/
#define MAX_SEM 128                      /* Definindo o número máximo de semáforos.    */
#define DB 0                             /* Define se está em modo de depuração.       */
#define DEBUG if (DB)                    /* Monta condição de depuração.               */

#include "fila.h"                        /* Inserção das funções de controle de fila.  */
/*?????????????????????????????????????????????????????????????????????????????????????*/
/*?????????????????????????????????????????????????????????????????????????????????????*/

#include "pm.h"
#include <minix/callnr.h>
#include <signal.h>
#include <sys/svrctl.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <string.h>
#include <machine/archtypes.h>
#include <lib.h>
#include <assert.h>
#include "mproc.h"
#include "param.h"
#include "kernel/proc.h"

PUBLIC struct utsname uts_val = {
  "Minix",		/* system name */
  "noname",		/* node/network name */
  OS_RELEASE,		/* O.S. release (e.g. 1.5) */
  OS_VERSION,		/* O.S. version (e.g. 10) */
  "xyzzy",		/* machine (cpu) type (filled in later) */
#if __i386
  "i386",		/* architecture */
#else
#error			/* oops, no 'uname -mk' */
#endif
};

PRIVATE char *uts_tbl[] = {
  uts_val.arch,
  NULL,			/* No kernel architecture */
  uts_val.machine,
  NULL,			/* No hostname */
  uts_val.nodename,
  uts_val.release,
  uts_val.version,
  uts_val.sysname,
  NULL,			/* No bus */			/* No bus */
};

#if ENABLE_SYSCALL_STATS
PUBLIC unsigned long calls_stats[NCALLS];
#endif

FORWARD _PROTOTYPE( int getpciinfo, (struct pciinfo *pciinfo)		);

/*===========================================================================*
 *				do_procstat				     *
 *===========================================================================*/
PUBLIC int do_procstat()
{ 
  /* For the moment, this is only used to return pending signals to 
   * system processes that request the PM for their own status. 
   *
   * Future use might include the FS requesting for process status of
   * any user process. 
   */
  
  /* This call should be removed, or made more general. */

  if (m_in.stat_nr == SELF) {
      mp->mp_reply.sig_set = mp->mp_sigpending;
      sigemptyset(&mp->mp_sigpending);
  } 
  else {
      return(ENOSYS);
  }
  return(OK);
}

/*===========================================================================*
 *				do_sysuname				     *
 *===========================================================================*/
PUBLIC int do_sysuname()
{
/* Set or get uname strings. */

  int r;
  size_t n;
  char *string;
#if 0 /* for updates */
  char tmp[sizeof(uts_val.nodename)];
  static short sizes[] = {
	0,	/* arch, (0 = read-only) */
	0,	/* kernel */
	0,	/* machine */
	0,	/* sizeof(uts_val.hostname), */
	sizeof(uts_val.nodename),
	0,	/* release */
	0,	/* version */
	0,	/* sysname */
  };
#endif

  if ((unsigned) m_in.sysuname_field >= _UTS_MAX) return(EINVAL);

  string = uts_tbl[m_in.sysuname_field];
  if (string == NULL)
	return EINVAL;	/* Unsupported field */

  switch (m_in.sysuname_req) {
  case _UTS_GET:
	/* Copy an uname string to the user. */
	n = strlen(string) + 1;
	if (n > m_in.sysuname_len) n = m_in.sysuname_len;
	r = sys_vircopy(SELF, D, (phys_bytes) string, 
		mp->mp_endpoint, D, (phys_bytes) m_in.sysuname_value,
		(phys_bytes) n);
	if (r < 0) return(r);
	break;

#if 0	/* no updates yet */
  case _UTS_SET:
	/* Set an uname string, needs root power. */
	len = sizes[m_in.sysuname_field];
	if (mp->mp_effuid != 0 || len == 0) return(EPERM);
	n = len < m_in.sysuname_len ? len : m_in.sysuname_len;
	if (n <= 0) return(EINVAL);
	r = sys_vircopy(mp->mp_endpoint, D, (phys_bytes) m_in.sysuname_value,
		SELF, D, (phys_bytes) tmp, (phys_bytes) n);
	if (r < 0) return(r);
	tmp[n-1] = 0;
	strcpy(string, tmp);
	break;
#endif

  default:
	return(EINVAL);
  }
  /* Return the number of bytes moved. */
  return(n);
}


/*===========================================================================*
 *				do_getsysinfo			       	     *
 *===========================================================================*/
PUBLIC int do_getsysinfo()
{
  struct mproc *proc_addr;
  vir_bytes src_addr, dst_addr;
  struct kinfo kinfo;
  struct loadinfo loadinfo;
  struct pciinfo pciinfo;
  static struct proc proctab[NR_PROCS+NR_TASKS];
  size_t len;
  int s, r;

  /* This call leaks important information (the contents of registers). */
  if (mp->mp_effuid != 0)
  {
	printf("PM: unauthorized call of do_getsysinfo by proc %d '%s'\n",
		mp->mp_endpoint, mp->mp_name);
	sys_sysctl_stacktrace(mp->mp_endpoint);
	return EPERM;
  }

  switch(m_in.info_what) {
  case SI_KINFO:			/* kernel info is obtained via PM */
        sys_getkinfo(&kinfo);
        src_addr = (vir_bytes) &kinfo;
        len = sizeof(struct kinfo);
        break;
  case SI_PROC_ADDR:			/* get address of PM process table */
  	proc_addr = &mproc[0];
  	src_addr = (vir_bytes) &proc_addr;
  	len = sizeof(struct mproc *);
  	break; 
  case SI_PROC_TAB:			/* copy entire process table */
        src_addr = (vir_bytes) mproc;
        len = sizeof(struct mproc) * NR_PROCS;
        break;
  case SI_KPROC_TAB:			/* copy entire process table */
	if((r=sys_getproctab(proctab)) != OK)
		return r;
	src_addr = (vir_bytes) proctab;
	len = sizeof(proctab);
        break;
  case SI_LOADINFO:			/* loadinfo is obtained via PM */
        sys_getloadinfo(&loadinfo);
        src_addr = (vir_bytes) &loadinfo;
        len = sizeof(struct loadinfo);
        break;
  case SI_PCI_INFO:			/* PCI info is obtained via PM */
        if ((r=getpciinfo(&pciinfo)) != OK)
			return r;
        src_addr = (vir_bytes) &pciinfo;
        len = sizeof(struct pciinfo);
        break;
#if ENABLE_SYSCALL_STATS
  case SI_CALL_STATS:
  	src_addr = (vir_bytes) calls_stats;
  	len = sizeof(calls_stats);
  	break; 
#endif
  default:
  	return(EINVAL);
  }

  dst_addr = (vir_bytes) m_in.info_where;
  if (OK != (s=sys_datacopy(SELF, src_addr, who_e, dst_addr, len)))
  	return(s);
  return(OK);
}

/*===========================================================================*
 *				do_getsysinfo_up		       	     *
 *===========================================================================*/
PUBLIC int do_getsysinfo_up()
{
  vir_bytes src_addr, dst_addr;
  struct loadinfo loadinfo;
  size_t len, real_len;
  u64_t idle_tsc;
  int s;

  switch(m_in.SIU_WHAT) {
  case SIU_LOADINFO:			/* loadinfo is obtained via PM */
        if ((s = sys_getloadinfo(&loadinfo)) != OK)
        	return s;
        src_addr = (vir_bytes) &loadinfo;
        real_len = sizeof(struct loadinfo);
        break;
  case SIU_SYSTEMHZ:
        src_addr = (vir_bytes) &system_hz;
        real_len = sizeof(system_hz);
	break;
  case SIU_IDLETSC:
	if ((s = sys_getidletsc(&idle_tsc)) != OK)
		return s;
	src_addr = (vir_bytes) &idle_tsc;
	real_len = sizeof(idle_tsc);
	break;
  default:
  	return(EINVAL);
  }

  /* Let application know what the length was. */
  len = real_len;
  if(len > m_in.SIU_LEN)
	len = m_in.SIU_LEN;

  dst_addr = (vir_bytes) m_in.SIU_WHERE;
  if (OK != (s=sys_datacopy(SELF, src_addr, who_e, dst_addr, len)))
  	return(s);
  return(real_len);
}

/*===========================================================================*
 *				do_getprocnr			             *
 *===========================================================================*/
PUBLIC int do_getprocnr()
{
  register struct mproc *rmp;
  static char search_key[PROC_NAME_LEN+1];
  int key_len;
  int s;

  /* This call should be moved to DS. */
  if (mp->mp_effuid != 0)
  {
	/* For now, allow non-root processes to request their own endpoint. */
	if (m_in.pid < 0 && m_in.namelen == 0) {
		mp->mp_reply.PM_ENDPT = who_e;
		mp->mp_reply.PM_PENDPT = NONE;
		return OK;
	}

	printf("PM: unauthorized call of do_getprocnr by proc %d\n",
		mp->mp_endpoint);
	sys_sysctl_stacktrace(mp->mp_endpoint);
	return EPERM;
  }

#if 0
  printf("PM: do_getprocnr(%d) call from endpoint %d, %s\n",
	m_in.pid, mp->mp_endpoint, mp->mp_name);
#endif

  if (m_in.pid >= 0) {			/* lookup process by pid */
	if ((rmp = find_proc(m_in.pid)) != NULL) {
		mp->mp_reply.PM_ENDPT = rmp->mp_endpoint;
#if 0
		printf("PM: pid result: %d\n", rmp->mp_endpoint);
#endif
		return(OK);
	}
  	return(ESRCH);			
  } else if (m_in.namelen > 0) {	/* lookup process by name */
  	key_len = MIN(m_in.namelen, PROC_NAME_LEN);
 	if (OK != (s=sys_datacopy(who_e, (vir_bytes) m_in.PMBRK_ADDR,
 			SELF, (vir_bytes) search_key, key_len))) 
 		return(s);
 	search_key[key_len] = '\0';	/* terminate for safety */
  	for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
		if (((rmp->mp_flags & (IN_USE | EXITING)) == IN_USE) && 
			strncmp(rmp->mp_name, search_key, key_len)==0) {
  			mp->mp_reply.PM_ENDPT = rmp->mp_endpoint;
  			return(OK);
		} 
	}
  	return(ESRCH);			
  } else {			/* return own/parent process number */
#if 0
	printf("PM: endpt result: %d\n", mp->mp_reply.PM_ENDPT);
#endif
  	mp->mp_reply.PM_ENDPT = who_e;
	mp->mp_reply.PM_PENDPT = mproc[mp->mp_parent].mp_endpoint;
  }

  return(OK);
}

/*===========================================================================*
 *				do_getepinfo			             *
 *===========================================================================*/
PUBLIC int do_getepinfo()
{
  register struct mproc *rmp;
  endpoint_t ep;

  /* This call should be moved to DS. */
  if (mp->mp_effuid != 0)
  {
	printf("PM: unauthorized call of do_getepinfo by proc %d\n",
		mp->mp_endpoint);
	sys_sysctl_stacktrace(mp->mp_endpoint);
	return EPERM;
  }

  ep= m_in.PM_ENDPT;

  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
	if ((rmp->mp_flags & IN_USE) && (rmp->mp_endpoint == ep)) {
		mp->mp_reply.reply_res2 = rmp->mp_effuid;
		mp->mp_reply.reply_res3 = rmp->mp_effgid;
		return(rmp->mp_pid);
	}
  } 

  /* Process not found */
  return(ESRCH);
}

/*===========================================================================*
 *				do_reboot				     *
 *===========================================================================*/
PUBLIC int do_reboot()
{
  message m;

  /* Check permission to abort the system. */
  if (mp->mp_effuid != SUPER_USER) return(EPERM);

  /* See how the system should be aborted. */
  abort_flag = (unsigned) m_in.reboot_flag;
  if (abort_flag >= RBT_INVALID) return(EINVAL); 
  if (RBT_MONITOR == abort_flag) {
	int r;
	if(m_in.reboot_strlen >= sizeof(monitor_code))
		return EINVAL;
	if((r = sys_datacopy(who_e, (vir_bytes) m_in.reboot_code,
		SELF, (vir_bytes) monitor_code, m_in.reboot_strlen)) != OK)
		return r;
	monitor_code[m_in.reboot_strlen] = '\0';
  }
  else
	monitor_code[0] = '\0';

  /* Order matters here. When FS is told to reboot, it exits all its
   * processes, and then would be confused if they're exited again by
   * SIGKILL. So first kill, then reboot. 
   */

  check_sig(-1, SIGKILL, FALSE /* ksig*/); /* kill all users except init */
  sys_stop(INIT_PROC_NR);		   /* stop init, but keep it around */

  /* Tell FS to reboot */
  m.m_type = PM_REBOOT;

  tell_fs(&mproc[FS_PROC_NR], &m);

  return(SUSPEND);			/* don't reply to caller */
}

/*===========================================================================*
 *				do_getsetpriority			     *
 *===========================================================================*/
PUBLIC int do_getsetpriority()
{
	int r, arg_which, arg_who, arg_pri;
	struct mproc *rmp;

	arg_which = m_in.m1_i1;
	arg_who = m_in.m1_i2;
	arg_pri = m_in.m1_i3;	/* for SETPRIORITY */

	/* Code common to GETPRIORITY and SETPRIORITY. */

	/* Only support PRIO_PROCESS for now. */
	if (arg_which != PRIO_PROCESS)
		return(EINVAL);

	if (arg_who == 0)
		rmp = mp;
	else
		if ((rmp = find_proc(arg_who)) == NULL)
			return(ESRCH);

	if (mp->mp_effuid != SUPER_USER &&
	   mp->mp_effuid != rmp->mp_effuid && mp->mp_effuid != rmp->mp_realuid)
		return EPERM;

	/* If GET, that's it. */
	if (call_nr == GETPRIORITY) {
		return(rmp->mp_nice - PRIO_MIN);
	}

	/* Only root is allowed to reduce the nice level. */
	if (rmp->mp_nice > arg_pri && mp->mp_effuid != SUPER_USER)
		return(EACCES);
	
	/* We're SET, and it's allowed.
	 *
	 * The value passed in is currently between PRIO_MIN and PRIO_MAX.
	 * We have to scale this between MIN_USER_Q and MAX_USER_Q to match
	 * the kernel's scheduling queues.
	 */

	if ((r = sched_nice(rmp, arg_pri)) != OK) {
		return r;
	}

	rmp->mp_nice = arg_pri;
	return(OK);
}

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
PUBLIC int do_svrctl()
{
  int s, req;
  vir_bytes ptr;
#define MAX_LOCAL_PARAMS 2
  static struct {
  	char name[30];
  	char value[30];
  } local_param_overrides[MAX_LOCAL_PARAMS];
  static int local_params = 0;

  req = m_in.svrctl_req;
  ptr = (vir_bytes) m_in.svrctl_argp;

  /* Is the request indeed for the MM? */
  if (((req >> 8) & 0xFF) != 'M') return(EINVAL);

  /* Control operations local to the PM. */
  switch(req) {
  case MMSETPARAM:
  case MMGETPARAM: {
      struct sysgetenv sysgetenv;
      char search_key[64];
      char *val_start;
      size_t val_len;
      size_t copy_len;

      /* Copy sysgetenv structure to PM. */
      if (sys_datacopy(who_e, ptr, SELF, (vir_bytes) &sysgetenv,
              sizeof(sysgetenv)) != OK) return(EFAULT);  

      /* Set a param override? */
      if (req == MMSETPARAM) {
  	if (local_params >= MAX_LOCAL_PARAMS) return ENOSPC;
  	if (sysgetenv.keylen <= 0
  	 || sysgetenv.keylen >=
  	 	 sizeof(local_param_overrides[local_params].name)
  	 || sysgetenv.vallen <= 0
  	 || sysgetenv.vallen >=
  	 	 sizeof(local_param_overrides[local_params].value))
  		return EINVAL;
  		
          if ((s = sys_datacopy(who_e, (vir_bytes) sysgetenv.key,
            SELF, (vir_bytes) local_param_overrides[local_params].name,
               sysgetenv.keylen)) != OK)
               	return s;
          if ((s = sys_datacopy(who_e, (vir_bytes) sysgetenv.val,
            SELF, (vir_bytes) local_param_overrides[local_params].value,
              sysgetenv.vallen)) != OK)
               	return s;
            local_param_overrides[local_params].name[sysgetenv.keylen] = '\0';
            local_param_overrides[local_params].value[sysgetenv.vallen] = '\0';

  	local_params++;

  	return OK;
      }

      if (sysgetenv.keylen == 0) {	/* copy all parameters */
          val_start = monitor_params;
          val_len = sizeof(monitor_params);
      } 
      else {				/* lookup value for key */
      	  int p;
          /* Try to get a copy of the requested key. */
          if (sysgetenv.keylen > sizeof(search_key)) return(EINVAL);
          if ((s = sys_datacopy(who_e, (vir_bytes) sysgetenv.key,
                  SELF, (vir_bytes) search_key, sysgetenv.keylen)) != OK)
              return(s);

          /* Make sure key is null-terminated and lookup value.
           * First check local overrides.
           */
          search_key[sysgetenv.keylen-1]= '\0';
          for(p = 0; p < local_params; p++) {
          	if (!strcmp(search_key, local_param_overrides[p].name)) {
          		val_start = local_param_overrides[p].value;
          		break;
          	}
          }
          if (p >= local_params && (val_start = find_param(search_key)) == NULL)
               return(ESRCH);
          val_len = strlen(val_start) + 1;
      }

      /* See if it fits in the client's buffer. */
      if (val_len > sysgetenv.vallen)
      	return E2BIG;

      /* Value found, make the actual copy (as far as possible). */
      copy_len = MIN(val_len, sysgetenv.vallen); 
      if ((s=sys_datacopy(SELF, (vir_bytes) val_start, 
              who_e, (vir_bytes) sysgetenv.val, copy_len)) != OK)
          return(s);

      return OK;
  }

  default:
	return(EINVAL);
  }
}

/*===========================================================================*
 *				_brk				             *
 *===========================================================================*/

extern char *_brksize;
PUBLIC int brk(brk_addr)
char *brk_addr;
{
	int r;
/* PM wants to call brk() itself. */
	if((r=vm_brk(PM_PROC_NR, brk_addr)) != OK) {
#if 0
		printf("PM: own brk(%p) failed: vm_brk() returned %d\n",
			brk_addr, r);
#endif
		return -1;
	}
	_brksize = brk_addr;
	return 0;
}

/*===========================================================================*
 *				getpciinfo				     *
 *===========================================================================*/

PRIVATE int getpciinfo(pciinfo)
struct pciinfo *pciinfo;
{
	int devind, r;
	struct pciinfo_entry *entry;
	char *name;
	u16_t vid, did;

	/* look up PCI process number */
	pci_init();

	/* start enumerating devices */
	entry = pciinfo->pi_entries;
	r = pci_first_dev(&devind, &vid, &did);
	while (r)
	{
		/* fetch device name */
		name = pci_dev_name(vid, did);
		if (!name)
			name = "";

		/* store device information in table */
		assert((char *) entry < (char *) (pciinfo + 1));
		entry->pie_vid = vid;
		entry->pie_did = did;
		strncpy(entry->pie_name, name, sizeof(entry->pie_name));
		entry->pie_name[sizeof(entry->pie_name) - 1] = 0;
		entry++;
		
		/* continue with the next device */
		r = pci_next_dev(&devind, &vid, &did);
	}
	
	/* store number of entries */
	pciinfo->pi_count = entry - pciinfo->pi_entries;
	return OK;
}

/*??????????????????????????????????????????????????*/
/*??????????????????????????????????????????????????*/

/*
 * Definindo a estrutura do semaforo.
 */
typedef struct semaforo {
	/* quantidade de processos que podem passar pelo semáforo           */
	int NMEM;           /* Tamanho do semaforo.                         */
	int N;              /* Posicoes livres do semaforo.                 */
	unsigned int ppid;  /* PID do processo que pediu este semaforo.     */
	short int espera;   /* Processo pai esperando o termino dos filhos. */
	fila *f;            /* Fila de acesso ao semaforo.                  */
} SEM;


/* Declarando variaveis globais.                                            */
static SEM vet_sem[MAX_SEM];        /* Vetor de semaforos.                  */
static short int qnt_sem = -1;		/* Quantidade de semáforos disponiveis. */

/*
 * Funcao utilizada para iniciar os semaforos na primeira vez que o sistema rodar.
 * Para isso a variavel que contem os semaforos livres que foi iniciada com -1
 * servira de contador para a inicializacao e terminara com MAX_SEM.
 */
void inicializa_sem(void) {
	DEBUG printf("\nIniciando os semaforos.\n");
	for( qnt_sem=0; qnt_sem<MAX_SEM; qnt_sem++) {
		vet_sem[qnt_sem].ppid = NO_PID;
		vet_sem[qnt_sem].f = novaFila();
		vet_sem[qnt_sem].espera = 0;
	}
}

/*
 * Funcão que retorna 1 caso parent seja ancestral de ppid.
 */
int ancestral (int ppid, int parent) {
	int prn = ppid;
	int mem = NO_PID;

	DEBUG printf("\n");
	while ( mem!=prn && prn>parent ) {
		DEBUG printf("name: %s\tpid1: %d\tpid2: %d\tparent: %d\n", mproc[prn].mp_name, prn, mproc[prn].mp_pid, mproc[prn].mp_parent );
		mem = prn;
		prn = mproc[prn].mp_parent;
	}
	if (prn==parent) return 1;
	return 0;
}

/*
 * Funcao que retorna uma nova posicao de semaforo valida.
 * Retorna -1 indicando erro quando o numero de semaforos
 * maximo e alcancado.
 */
PUBLIC int do_get_sem() {
    int i;
    int n = m_in.m1_i1;     /* Tamanho do semaforo. */
    int ppid = who_p;       /* Pid do pai.          */

	/* Verificando se vetor de semaforos precisa ser inicializado. */
	if ( qnt_sem == -1 ) inicializa_sem();

	/* não há mais semáforos disponíveis */
	if (qnt_sem==0 || n<=0){
		DEBUG printf("\nSemaforos indisponiveis, matando o processo.\n");
		return -1;
	} else {
		DEBUG printf("\nProcurando por uma posicao livre no vetor de semaforos.\n");
		for( i=0; i<MAX_SEM; i++) {
			if ( vet_sem[i].ppid == NO_PID ) {
				qnt_sem--;
				vet_sem[i].N = n;
				vet_sem[i].NMEM = n;
				vet_sem[i].ppid = ppid;
				DEBUG printf("\nAlocando espaço na posicao %d\n", i);
				return i;
			}
		}
		DEBUG printf("\nErro no vetor de semaforos.\n");
		return -2;
	}
}

/*
 * Funcao que faz um P para o semaforo, caso o processo que faz o pedido nao seja descendente
 * do processo que pediu o semaforo sera -1 indicando erro. Caso tenha acesso garantido sera
 * retornado 1 e caso seja necessario esperar retorna SUSPEND.
 */
PUBLIC int do_p_sem() 
{
	int ppid = _ENDPOINT_P(m_in.m_source);       /* Pid de quem chamou a call.             */
	int sid = m_in.m1_i1;                        /* Sid para a qual o proceso pede acesso. */
	SEM *sem = vet_sem+sid;                      /* Auxiliar para o caminho do semaforo.   */

	DEBUG printf("\nProcesso %d tentando acessar o semaforo %d.\n", ppid, sid);

	if ( !ancestral(ppid, sem->ppid) ) {
		DEBUG printf("\nProcesso %d acessando indevidamente semaforo %d. Matando processo. \n", ppid, sid);
		return -1;
	}

	--sem->N;
	/* se  puder passar */
	if (sem->N >= 0) {
		DEBUG printf("\nProcesso %d acessando diretamente o semaforo %d.\n", ppid, sid);
		return 1;
	}

	DEBUG printf("\nProcesso %d aguardando o semaforo %d.\n", ppid, sid);
	entra ( vet_sem[sid].f , ppid );
	return (SUSPEND);
}

/*
 * Funcao que faz o P do semaforo. Caso exista alguem esperando na fila de acesso acorda este processo.
 * Se o processo gerador deste semaforo estiver esperando os filhos terminarem e o processo que chamou
 * for o ultimo na fila avisa o processo gerador que pode terminar.
 * Caso algum processo tente liberar um semaforo que nao e descendente retorna -1 indicando erro.
 */
PUBLIC int do_v_sem()
{
	int ppid = _ENDPOINT_P(m_in.m_source);
	int npid;
	int sid = m_in.m1_i1;
	SEM *sem = vet_sem+sid;

	DEBUG printf("\nProcesso %d liberando o semaforo %d.\n", ppid, sid);

	if ( !ancestral(ppid, sem->ppid) ) {
		DEBUG printf("\nProcesso %d liberando indevidamente semaforo %d. Matando processo. \n", ppid, sid);
		return -1;
	}

	++sem->N;
	/* Acordando o próximo a entrar no semaforo. */
	if ( tamanho(sem->f) > 0) {
		npid = proximo(sem->f);
		DEBUG printf("\nProcesso %d acordando processo %d no semaforo %d.\n", ppid, npid, sid);
		setreply (npid, 1);
	}
	/* Liberando o pai caso ele esteja esperando a conclusao dos filhos. */
	else {
		if ( sem->N==sem->NMEM && sem->espera ){
			DEBUG printf("\nProcesso %d avisando pai %d do final da fila do semaforo %d.\n", ppid, vet_sem[sid].ppid, sid);
			setreply(vet_sem[sid].ppid, 1);
		}
	}

	return 0;
}

/*
 * Funcao que libera um semaforo. Pode ser usada diretamente pelo processo gerador
 * como quando o processo gerador morre. Caso um processo que nao tenha gerado este
 * semaforo tente cancela-lo a funcao retorna -1 indicando erro.
 */
int free_sem(int sid, int ppid)
{
	if ( vet_sem[sid].ppid==ppid ) {
		vet_sem[sid].ppid = NO_PID;
		if ( fechou(vet_sem[sid].f) )
			vet_sem[sid].f = novaFila();
		vet_sem[sid].espera = 0;
		qnt_sem++;
		return 0;
	}
	return -1;
}
/*
 * Funcao que trata a call para liberar um semaforo.
 * Apenas uma interface de acesso para a funcao free_sem().
 */
PUBLIC int do_free_sem()
{
	int sid = m_in.m1_i1;
	int ppid = who_p;

	DEBUG printf("\nProcesso %d pede cancelamento do semaforo %d.\n", ppid, sid);

	return free_sem(sid, ppid);
}

/*
 * Funcao chamada pelo PM para verificar se um processo morto gerou algum semaforo.
 * Se sim libera o semaforo para outros processos.
 */
void terminator(int ppid) {
	int i;
	for ( i=0; i<MAX_SEM; i++ ) {
		if ( vet_sem[i].ppid == ppid ) {
			DEBUG printf("\nProcesso %d morreu, cancelando semaforo %d.\n", ppid, i);
			free_sem( i, ppid);
		}
	}
}

/*
 * Funcao que permite que o processo gerador do semaforo espere que todos os seus descendentes
 * liberem o acesso para o semaforo.
 */
PUBLIC int do_wait_sem(void) {
	int ppid = who_p;
	int sid = m_in.m1_i1;
	if ( vet_sem[sid].ppid == ppid ) {
		vet_sem[sid].espera = 1;
		return (SUSPEND);
	}
	return -1;
}
/*??????????????????????????????????????????????????*/
/*??????????????????????????????????????????????????*/
