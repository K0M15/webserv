#include "../src/PollHandler.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

static int g_passed = 0;
static int g_failed = 0;

static void pass(const char* name) {
	std::cout << "[SUCCESS] " << name << std::endl;
	++g_passed;
}

static void fail(const char* name, const char* msg) {
	std::cout << "[FAILURE] " << name;
	if (msg && msg[0]) std::cout << " - " << msg;
	std::cout << std::endl;
	++g_failed;
}

static void check(const char* name, bool condition, const char* fail_msg = "") {
	if (condition)
		pass(name);
	else
		fail(name, fail_msg);
}

static fvoid_t make_dummy() { return []() {}; }

// --------------- construction / copy ---------------

static void test_constructor_default() {
	PollHandler ph;
	check("default constructor: getTimeout() == 3000", ph.getTimeout() == 3000);
	check("default constructor: getEventByFD returns null for empty", ph.getEventByFD(0) == nullptr);
}

static void test_constructor_custom() {
	PollHandler ph(5000);
	check("custom constructor: getTimeout() == 5000", ph.getTimeout() == 5000);
}

static void test_copy_constructor() {
	PollHandler a(2000);
	bool called = false;
	a.subscribe_read(10, nullptr, [&]() { called = true; });

	PollHandler b(a);
	check("copy constructor: timeout copied", b.getTimeout() == 2000);
	PollHandler::EventType* ev = b.getEventByFD(10);
	check("copy constructor: event copied", ev != nullptr);
	if (ev)
		check("copy constructor: on_readable copied", ev->on_readable != nullptr);

	a.unsubscribe(10);
	check("copy constructor: independent events", b.getEventByFD(10) != nullptr);
}

static void test_assignment_operator() {
	PollHandler a(1000);
	a.subscribe_read(20, nullptr, make_dummy());

	PollHandler b(3000);
	b.subscribe_write(30, nullptr, make_dummy());

	b = a;
	check("assignment: timeout copied", b.getTimeout() == 1000);
	check("assignment: src event present", b.getEventByFD(20) != nullptr);
	check("assignment: old event removed", b.getEventByFD(30) == nullptr);
}

static void test_self_assignment() {
	PollHandler a(1000);
	a.subscribe_read(40, nullptr, make_dummy());
	a = a;
	check("self-assignment: timeout preserved", a.getTimeout() == 1000);
	check("self-assignment: event preserved", a.getEventByFD(40) != nullptr);
}

// --------------- subscribe / getEventByFD ---------------

static void test_subscribe_new_read() {
	PollHandler ph;
	fvoid_t close_fn = make_dummy();
	fvoid_t read_fn = make_dummy();

	ph.subscribe_read(1, close_fn, read_fn);

	PollHandler::EventType* ev = ph.getEventByFD(1);
	check("subscribe_read: event created", ev != nullptr);
	if (!ev) return;
	check("subscribe_read: fd == 1", ev->fd == 1);
	check("subscribe_read: on_close set", ev->on_close != nullptr);
	check("subscribe_read: on_readable set", ev->on_readable != nullptr);
	check("subscribe_read: on_writeable null", ev->on_writeable == nullptr);
	check("subscribe_read: POLLIN set", (ev->event & POLLIN) != 0);
	check("subscribe_read: close flags set", (ev->event & (POLLERR | POLLHUP | POLLNVAL)) != 0);
	check("subscribe_read: POLLOUT not set", (ev->event & POLLOUT) == 0);
}

static void test_subscribe_new_write() {
	PollHandler ph;
	fvoid_t close_fn = make_dummy();
	fvoid_t write_fn = make_dummy();

	ph.subscribe_write(2, close_fn, write_fn);

	PollHandler::EventType* ev = ph.getEventByFD(2);
	check("subscribe_write: event created", ev != nullptr);
	if (!ev) return;
	check("subscribe_write: on_close set", ev->on_close != nullptr);
	check("subscribe_write: on_readable null", ev->on_readable == nullptr);
	check("subscribe_write: on_writeable set", ev->on_writeable != nullptr);
	check("subscribe_write: POLLOUT set", (ev->event & POLLOUT) != 0);
}

static void test_subscribe_new_both() {
	PollHandler ph;
	fvoid_t close_fn = make_dummy();
	fvoid_t read_fn = make_dummy();
	fvoid_t write_fn = make_dummy();

	ph.subscribe(3, close_fn, read_fn, write_fn);

	PollHandler::EventType* ev = ph.getEventByFD(3);
	check("subscribe_both: event created", ev != nullptr);
	if (!ev) return;
	check("subscribe_both: on_close set", ev->on_close != nullptr);
	check("subscribe_both: on_readable set", ev->on_readable != nullptr);
	check("subscribe_both: on_writeable set", ev->on_writeable != nullptr);
	check("subscribe_both: POLLIN set", (ev->event & POLLIN) != 0);
	check("subscribe_both: POLLOUT set", (ev->event & POLLOUT) != 0);
	check("subscribe_both: close flags set", (ev->event & (POLLERR | POLLHUP | POLLNVAL)) != 0);
}

static void test_subscribe_no_callbacks_throws() {
	PollHandler ph;
	bool threw = false;
	try {
		ph.subscribe(4, nullptr, nullptr, nullptr);
	} catch (const HttpServerException&) {
		threw = true;
	}
	check("subscribe: no callbacks throws", threw);
}

static void test_subscribe_update_add_write() {
	PollHandler ph;
	fvoid_t read_fn = make_dummy();
	fvoid_t write_fn = make_dummy();

	ph.subscribe_read(5, nullptr, read_fn);
	ph.subscribe_write(5, nullptr, write_fn);

	PollHandler::EventType* ev = ph.getEventByFD(5);
	check("subscribe_update: event exists", ev != nullptr);
	if (!ev) return;
	check("subscribe_update: on_readable preserved", ev->on_readable != nullptr);
	check("subscribe_update: on_writeable added", ev->on_writeable != nullptr);
	check("subscribe_update: on_close still null", ev->on_close == nullptr);
	check("subscribe_update: POLLIN set", (ev->event & POLLIN) != 0);
	check("subscribe_update: POLLOUT set", (ev->event & POLLOUT) != 0);
	check("subscribe_update: no close flags", (ev->event & (POLLERR | POLLHUP | POLLNVAL)) == 0);
}

static void test_subscribe_update_null_preserves() {
	PollHandler ph;
	fvoid_t read_fn = make_dummy();
	fvoid_t close_fn = make_dummy();

	ph.subscribe_read(6, close_fn, read_fn);
	ph.subscribe(6, nullptr, nullptr, nullptr);

	PollHandler::EventType* ev = ph.getEventByFD(6);
	check("update_null: event exists", ev != nullptr);
	if (!ev) return;
	check("update_null: on_close preserved", ev->on_close != nullptr);
	check("update_null: on_readable preserved", ev->on_readable != nullptr);
	check("update_null: POLLIN preserved", (ev->event & POLLIN) != 0);
	check("update_null: close flags preserved", (ev->event & (POLLERR | POLLHUP | POLLNVAL)) != 0);
}

static void test_subscribe_update_replace_read() {
	PollHandler ph;
	bool old_called = false;
	bool new_called = false;

	ph.subscribe_read(7, nullptr, [&]() { old_called = true; });
	ph.subscribe(7, nullptr, [&]() { new_called = true; }, nullptr);

	PollHandler::EventType* ev = ph.getEventByFD(7);
	check("update_replace: event exists", ev != nullptr);
	if (ev && ev->on_readable) {
		ev->on_readable();
		check("update_replace: new callback called", new_called);
		check("update_replace: old callback NOT called", !old_called);
	}
}

static void test_getEventByFD_found() {
	PollHandler ph;
	ph.subscribe_read(8, nullptr, make_dummy());
	PollHandler::EventType* ev = ph.getEventByFD(8);
	check("getEventByFD: found non-null", ev != nullptr);
	if (ev)
		check("getEventByFD: correct fd", ev->fd == 8);
}

static void test_getEventByFD_not_found() {
	PollHandler ph;
	check("getEventByFD: not found returns null", ph.getEventByFD(999) == nullptr);
}

// --------------- unsubscribe ---------------

static void test_unsubscribe() {
	PollHandler ph;
	ph.subscribe_read(9, nullptr, make_dummy());
	ph.subscribe_read(10, nullptr, make_dummy());
	ph.unsubscribe(9);
	check("unsubscribe: removed fd gone", ph.getEventByFD(9) == nullptr);
	check("unsubscribe: other fd still present", ph.getEventByFD(10) != nullptr);
}

static void test_unsubscribe_nonexistent() {
	PollHandler ph;
	ph.subscribe_read(11, nullptr, make_dummy());
	ph.unsubscribe(999);
	check("unsubscribe: nonexistent no-op", ph.getEventByFD(11) != nullptr);
}

static void test_unsubscribe_all() {
	PollHandler ph;
	ph.subscribe_read(12, nullptr, make_dummy());
	ph.subscribe_read(13, nullptr, make_dummy());
	ph.unsubscribe(12);
	ph.unsubscribe(13);
	check("unsubscribe_all: first gone", ph.getEventByFD(12) == nullptr);
	check("unsubscribe_all: second gone", ph.getEventByFD(13) == nullptr);
}

// --------------- checkFDs with real file descriptors ---------------

struct Pipe {
	int fds[2];
	Pipe() {
		if (pipe(fds) != 0) {
			std::cerr << "pipe() failed" << std::endl;
			fds[0] = -1;
			fds[1] = -1;
		}
	}
	~Pipe() {
		if (fds[0] >= 0) ::close(fds[0]);
		if (fds[1] >= 0) ::close(fds[1]);
	}
	int r() const { return fds[0]; }
	int w() const { return fds[1]; }
};

static void test_checkFDs_empty() {
	PollHandler ph(10);
	ph.checkFDs();
	check("checkFDs: empty events returns immediately", true);
}

static void test_checkFDs_readable() {
	Pipe p;
	if (p.r() < 0) {
		fail("checkFDs readable", "pipe creation failed");
		return;
	}

	PollHandler ph(100);
	bool fired = false;
	int call_count = 0;

	ph.subscribe_read(p.r(),
		nullptr,
		[&]() {
			fired = true;
			++call_count;
		}
	);

	::write(p.w(), "hello", 5);
	ph.checkFDs();

	check("checkFDs: readable callback fired", fired);
	check("checkFDs: readable callback once", call_count == 1);
}

static void test_checkFDs_writable() {
	Pipe p;
	if (p.w() < 0) {
		fail("checkFDs writable", "pipe creation failed");
		return;
	}

	PollHandler ph(100);
	bool fired = false;

	ph.subscribe_write(p.w(),
		nullptr,
		[&]() { fired = true; }
	);

	ph.checkFDs();
	check("checkFDs: writable callback fired", fired);
}

static void test_checkFDs_close_hup() {
	int fds[2];
	if (pipe(fds) != 0) {
		fail("checkFDs close", "pipe creation failed");
		return;
	}

	PollHandler ph(100);
	bool close_called = false;
	bool read_called = false;

	ph.subscribe_read(fds[0],
		[&]() { close_called = true; },
		[&]() { read_called = true; }
	);

	::close(fds[1]);
	ph.checkFDs();

	check("checkFDs: close callback fired on HUP", close_called);
	check("checkFDs: readable NOT fired (close priority)", !read_called);

	::close(fds[0]);
}

static void test_checkFDs_nval() {
	int fds[2];
	if (pipe(fds) != 0) {
		fail("checkFDs NVAL", "pipe creation failed");
		return;
	}

	PollHandler ph(100);
	bool close_called = false;

	ph.subscribe(fds[0],
		[&]() { close_called = true; },
		nullptr,
		nullptr
	);

	::close(fds[0]);
	::close(fds[1]);
	ph.checkFDs();

	check("checkFDs: NVAL fires close callback", close_called);
}

static void test_checkFDs_timeout() {
	Pipe p;
	if (p.r() < 0) {
		fail("checkFDs timeout", "pipe creation failed");
		return;
	}

	PollHandler ph(1);
	bool fired = false;
	int call_count = 0;

	ph.subscribe_read(p.r(),
		[&]() { fired = true; ++call_count; },
		[&]() { fired = true; ++call_count; }
	);

	ph.checkFDs();

	check("checkFDs: timeout - no callback fired", !fired);
	check("checkFDs: timeout - call count 0", call_count == 0);
}

static void test_checkFDs_unsubscribe_during_callback() {
	int fds[2];
	if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
		fail("checkFDs unsubscribe during callback", "socketpair creation failed");
		return;
	}

	PollHandler ph(100);
	bool read_called = false;
	bool write_called = false;

	ph.subscribe(fds[0],
		nullptr,
		[&]() {
			read_called = true;
			ph.unsubscribe(fds[0]);
		},
		[&]() {
			write_called = true;
		}
	);

	::write(fds[1], "x", 1);
	ph.checkFDs();

	check("checkFDs: readable callback fired before unsub", read_called);
	check("checkFDs: writable callback NOT fired (fd removed by read cb)", !write_called);
	check("checkFDs: fd no longer registered", ph.getEventByFD(fds[0]) == nullptr);

	::close(fds[0]);
	::close(fds[1]);
}

static void test_checkFDs_multiple_fds() {
	Pipe p1;
	Pipe p2;
	if (p1.r() < 0 || p2.r() < 0) {
		fail("checkFDs multiple fds", "pipe creation failed");
		return;
	}

	PollHandler ph(100);
	bool p1_fired = false;
	bool p2_fired = false;

	ph.subscribe_read(p1.r(), nullptr, [&]() { p1_fired = true; });
	ph.subscribe_read(p2.r(), nullptr, [&]() { p2_fired = true; });

	::write(p1.w(), "a", 1);
	::write(p2.w(), "b", 1);
	ph.checkFDs();

	check("checkFDs: multiple - p1 fired", p1_fired);
	check("checkFDs: multiple - p2 fired", p2_fired);
}

static void test_checkFDs_trigger_read_multiple() {
	Pipe p;
	if (p.r() < 0) {
		fail("checkFDs trigger read multiple", "pipe creation failed");
		return;
	}

	PollHandler ph(100);
	int count = 0;

	ph.subscribe_read(p.r(), nullptr, [&]() { ++count; });

	::write(p.w(), "first", 5);
	ph.checkFDs();
	check("checkFDs: first checkFDs fires once", count == 1);

	::write(p.w(), "second", 6);
	ph.checkFDs();
	check("checkFDs: second checkFDs fires again", count == 2);
}

static void test_checkFDs_close_ends_iteration() {
	int fds[2];
	if (pipe(fds) != 0) {
		fail("checkFDs close ends iteration", "pipe creation failed");
		return;
	}

	int fds2[2];
	if (pipe(fds2) != 0) {
		::close(fds[0]);
		::close(fds[1]);
		fail("checkFDs close ends iteration", "second pipe failed");
		return;
	}

	PollHandler ph(100);
	bool close_called = false;
	bool read_called = false;
	bool other_called = false;

	ph.subscribe_read(fds[0],
		[&]() { close_called = true; },
		[&]() { read_called = true; }
	);
	ph.subscribe_read(fds2[0],
		nullptr,
		[&]() { other_called = true; }
	);

	::close(fds[1]);
	::write(fds2[1], "data", 4);
	ph.checkFDs();

	check("checkFDs: close continues to next fd", other_called);
	check("checkFDs: close callback fired on hup", close_called);
	check("checkFDs: readable not fired on hup fd", !read_called);

	::close(fds[0]);
	::close(fds2[0]);
	::close(fds2[1]);
}

// --------------- main ---------------

int main() {
	std::cout << "--- PollHandler Tests ---" << std::endl;
	std::cout << std::endl;

	std::cout << "Construction & Copy:" << std::endl;
	test_constructor_default();
	test_constructor_custom();
	test_copy_constructor();
	test_assignment_operator();
	test_self_assignment();

	std::cout << "\nSubscribe & getEventByFD:" << std::endl;
	test_subscribe_new_read();
	test_subscribe_new_write();
	test_subscribe_new_both();
	test_subscribe_no_callbacks_throws();
	test_subscribe_update_add_write();
	test_subscribe_update_null_preserves();
	test_subscribe_update_replace_read();
	test_getEventByFD_found();
	test_getEventByFD_not_found();

	std::cout << "\nUnsubscribe:" << std::endl;
	test_unsubscribe();
	test_unsubscribe_nonexistent();
	test_unsubscribe_all();

	std::cout << "\ncheckFDs (I/O):" << std::endl;
	test_checkFDs_empty();
	test_checkFDs_readable();
	test_checkFDs_writable();
	test_checkFDs_close_hup();
	test_checkFDs_nval();
	test_checkFDs_timeout();
	test_checkFDs_unsubscribe_during_callback();
	test_checkFDs_multiple_fds();
	test_checkFDs_trigger_read_multiple();
	test_checkFDs_close_ends_iteration();

	std::cout << "\n----------------------------------------" << std::endl;
	std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
	          << (g_passed + g_failed) << " total" << std::endl;

	return (g_failed > 0) ? 1 : 0;
}
