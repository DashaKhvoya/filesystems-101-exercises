package parhash

import (
	"context"
	"net"
	"sync"
	"sync/atomic"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	"github.com/pkg/errors"
	"golang.org/x/sync/semaphore"

	"fs101ex/pkg/gen/hashsvc"
	"fs101ex/pkg/gen/parhashsvc"
	"fs101ex/pkg/workgroup"
)

type Config struct {
	ListenAddr   string
	BackendAddrs []string
	Concurrency  int
}

// Implement a server that responds to ParallelHash()
// as declared in /proto/parhash.proto.
//
// The implementation of ParallelHash() must not hash the content
// of buffers on its own. Instead, it must send buffers to backends
// to compute hashes. Buffers must be fanned out to backends in the
// round-robin fashion.
//
// For example, suppose that 2 backends are configured and ParallelHash()
// is called to compute hashes of 5 buffers. In this case it may assign
// buffers to backends in this way:
//
//	backend 0: buffers 0, 2, and 4,
//	backend 1: buffers 1 and 3.
//
// Requests to hash individual buffers must be issued concurrently.
// Goroutines that issue them must run within /pkg/workgroup/Wg. The
// concurrency within workgroups must be limited by Server.sem.
//
// WARNING: requests to ParallelHash() may be concurrent, too.
// Make sure that the round-robin fanout works in that case too,
// and evenly distributes the load across backends.
type Server struct {
	conf Config

	wg   sync.WaitGroup
	stop context.CancelFunc
	l    net.Listener
	sem  *semaphore.Weighted

	// Round Robin clients
	currClient  atomic.Uint64
	conns       []*grpc.ClientConn
	hashClients []hashsvc.HashSvcClient
}

func New(conf Config) *Server {
	sem := semaphore.NewWeighted(int64(conf.Concurrency))

	return &Server{
		conf: conf,
		sem:  sem,
	}
}

func (s *Server) Start(ctx context.Context) (err error) {
	defer func() { err = errors.Wrap(err, "Start()") }()

	if err = s.createHashClients(); err != nil {
		return err
	}

	ctx, s.stop = context.WithCancel(ctx)

	s.l, err = net.Listen("tcp", s.conf.ListenAddr)
	if err != nil {
		return err
	}

	srv := grpc.NewServer()
	parhashsvc.RegisterParallelHashSvcServer(srv, s)

	s.wg.Add(2)
	go func() {
		defer s.wg.Done()

		srv.Serve(s.l)
	}()
	go func() {
		defer s.wg.Done()

		<-ctx.Done()
		s.l.Close()
	}()

	return nil
}

func (s *Server) ListenAddr() string {
	return s.l.Addr().String()
}

func (s *Server) Stop() {
	s.stop()
	for _, conn := range s.conns {
		conn.Close()
	}
	s.wg.Wait()
}

func (s *Server) createHashClients() error {
	for _, addr := range s.conf.BackendAddrs {
		// TODO add clearing conn
		conn, err := grpc.Dial(addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
		if err != nil {
			return err
		}

		client := hashsvc.NewHashSvcClient(conn)
		s.conns = append(s.conns, conn)
		s.hashClients = append(s.hashClients, client)
	}

	return nil
}

func (s *Server) ParallelHash(ctx context.Context, req *parhashsvc.ParHashReq) (*parhashsvc.ParHashResp, error) {
	workers := workgroup.New(workgroup.Config{Sem: s.sem})
	hashes := make([][]byte, len(req.Data))

	for i, buffer := range req.Data {
		currSubReq := i
		currBuff := buffer

		workers.Go(ctx, func(ctx context.Context) error {
			client := s.getClient(ctx)
			resp, err := client.Hash(ctx, &hashsvc.HashReq{Data: currBuff})
			if err != nil {
				return err
			}

			hashes[currSubReq] = resp.GetHash()
			return nil
		})
	}

	err := workers.Wait()
	if err != nil {
		return nil, err
	}

	return &parhashsvc.ParHashResp{Hashes: hashes}, nil
}

func (s *Server) getClient(ctx context.Context) hashsvc.HashSvcClient {
	acquired := s.currClient.Add(1) % uint64(len(s.hashClients))

	return s.hashClients[acquired]
}
