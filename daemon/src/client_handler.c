#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include "iosched/types.h"
#include "daemon/client_handler.h"

int ch_init(client_handler_t *ch, scheduler_t *sched) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ch->clients[i].fd = -1;
        ch->clients[i].cid = 0;
    }
    ch->client_cnt = 0;

    ch->sched = sched;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;

    unlink(IPC_SOCKET_PATH);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        close(fd);
        return -1;
    }
    
    if (listen(fd, IPC_MAX_BACKLOG)) {
        close(fd);
        unlink(IPC_SOCKET_PATH);
        return -1;
    }

    ch->listen_fd = fd;

    return 0;
}

void ch_destroy(client_handler_t *ch) {
    /* 1. 연결된 모든 클라이언트에 대해 ch_disconnect 호출 */

    /* 2. listen_fd close */

    /* 3. 소켓 파일 unlink (IPC_SOCKET_PATH) */

    /* 4. listen_fd = -1 로 무효화 */
}

int ch_accept(client_handler_t *ch) {
    /* 1. accept()로 새 연결 수락 → client_fd 획득 */
    

    /* 2. 클라이언트 수 MAX_CLIENTS 초과 확인 → 초과 시 fd close 후 에러 반환 */

    /* 3. 클라이언트 테이블에서 빈 슬롯(fd == -1) 탐색 */

    /* 4. 슬롯에 fd 저장, cid 할당 (슬롯 인덱스 + 1 등), client_cnt 증가 */

    /* 5. 성공 반환 (슬롯 인덱스) */
}

void ch_disconnect(client_handler_t *ch, int idx) {
    /* 1. idx 범위 및 슬롯 유효성(fd != -1) 검증 */

    /* 2. 해당 클라이언트의 대기 요청 제거 — rq_remove_by_cid(cid) */

    /* 3. 클라이언트 소켓 close */

    /* 4. 슬롯 초기화 (fd=-1, cid=0, app_name 클리어) */

    /* 5. client_cnt 감소 */
}

int ch_handle_message(client_handler_t *ch, int idx) {
    /* 1. 클라이언트 fd에서 ipc_header_t 먼저 read */

    /* 2. header.type에 따라 분기:
     *
     *    IPC_CONNECT:
     *      a. 나머지 바디(ipc_connect_t) read — app_name 추출
     *      b. 슬롯의 app_name에 저장
     *      c. ipc_connect_ack_t 응답 전송 (status=OK, cid 포함)
     *
     *    IPC_IO_REQUEST:
     *      a. 나머지 바디(ipc_io_request_t) read
     *      b. io_request_t 할당 + 필드 변환 (wire → internal)
     *      c. sched_submit()으로 스케줄러에 제출
     *      d. 큐 full 등 에러 시 즉시 ipc_io_response_t 에러 응답
     *
     *    IPC_DISCONNECT:
     *      a. ch_disconnect() 호출
     *
     *    IPC_GET_STATUS:
     *      a. scheduler 상태 수집 (policy, queue len, total_reqs 등)
     *      b. ipc_status_report_t 응답 전송
     *
     *    그 외: 무시 또는 에러 응답
     */

    /* 3. read 실패(0 또는 -1) 시 — 클라이언트 비정상 종료로 간주, ch_disconnect */
}

int ch_poll(client_handler_t *ch) {
    /* 1. pollfd 배열 구성 — [0]=listen_fd, [1..N]=활성 클라이언트 fd, 이벤트=POLLIN */

    /* 2. poll() 호출 (타임아웃은 호출자가 결정하도록 파라미터화 또는 -1 블로킹) */

    /* 3. poll 에러 처리 — EINTR면 재시도, 그 외 에러 반환 */

    /* 4. listen_fd에 이벤트 → ch_accept() 호출 */

    /* 5. 각 클라이언트 fd에 이벤트 → ch_handle_message() 호출 */

    /* 6. 처리한 이벤트 수 반환 */
}