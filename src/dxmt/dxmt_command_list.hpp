#pragma once
#include <concepts>
#include <functional>
#include <chrono>
#include <cstdio>
#include <typeinfo>

namespace dxmt {

template <typename F, typename context>
concept CommandWithContext = requires(F f, context &ctx) {
  { f(ctx) } -> std::same_as<void>;
};

namespace impl {
template <typename context> class CommandBase {
public:
  virtual void invoke(context &) = 0;
  virtual ~CommandBase() noexcept {};
  CommandBase<context> *next = nullptr;
};

template <typename context, typename F> class LambdaCommand final : public CommandBase<context> {
public:
  void
  invoke(context &ctx) final {
    std::invoke(func, ctx);
  };
  ~LambdaCommand() noexcept final = default;
  LambdaCommand(F &&ff) : CommandBase<context>(), func(std::forward<F>(ff)) {}
  LambdaCommand(const LambdaCommand &copy) = delete;
  LambdaCommand &operator=(const LambdaCommand &copy_assign) = delete;

private:
  F func;
};

template <typename context> class EmtpyCommand final : public CommandBase<context> {
public:
  void invoke(context &ctx) final{/* nop */};
  ~EmtpyCommand() noexcept = default;
};

template <typename value_t> struct LinkedListNode {
  value_t value;
  LinkedListNode *next;
};

} // namespace impl

template <typename Context> class CommandList {

  impl::EmtpyCommand<Context> empty;
  impl::CommandBase<Context> *list_end;

public:
  CommandList() : list_end(&empty) {
    empty.next = nullptr;
  }
  ~CommandList() {
    reset();
  }

  void
  reset() {
    impl::CommandBase<Context> *cur = empty.next;
    while (cur) {
      auto next = cur->next;
      cur->~CommandBase<Context>(); // call destructor
      cur = next;
    }
    empty.next = nullptr;
    list_end = &empty;
  }

  CommandList(const CommandList &copy) = delete;
  CommandList(CommandList &&move) {
    this->reset();
    empty.next = move.empty.next;
    list_end = move.list_end;
    move.empty.next = nullptr;
    move.list_end = nullptr;
  }

  CommandList& operator=(CommandList&& move) {
    this->reset();
    empty.next = move.empty.next;
    list_end = move.list_end;
    move.empty.next = nullptr;
    move.list_end = nullptr;
    return *this;
  }

  template <CommandWithContext<Context> Fn>
  constexpr unsigned
  calculateCommandSize() {
    using command_t = impl::LambdaCommand<Context, Fn>;
    return sizeof(command_t);
  }

  template <CommandWithContext<Context> Fn>
  unsigned
  emit(Fn &&cmd, void *buffer) {
    using command_t = impl::LambdaCommand<Context, Fn>;
    auto cmd_h = new (buffer) command_t(std::forward<Fn>(cmd));
    list_end->next = cmd_h;
    list_end = cmd_h;
    return sizeof(command_t);
  }

  void append(CommandList &&list) {
    list_end->next = list.empty.next;
    list_end = list.list_end;
    list.empty.next = nullptr;
    list.list_end = nullptr;
  }

  void
  execute(Context &context) {
#ifdef DXMT_PERF
    // d9mt diagnostics: when a chunk takes >50ms, log which command types cost the time.
    using clk = std::chrono::steady_clock;
    struct Agg { const char *name; double ms; unsigned n; };
    Agg agg[32]; unsigned nagg = 0;
    auto t_begin = clk::now();
    impl::CommandBase<Context> *cur = &empty;
    while (cur) {
      auto t0 = clk::now();
      cur->invoke(context);
      double ms = std::chrono::duration<double, std::milli>(clk::now() - t0).count();
      const char *nm = typeid(*cur).name();
      unsigned k = 0;
      for (; k < nagg; k++) if (agg[k].name == nm) break;
      if (k == nagg && nagg < 32) agg[nagg++] = {nm, 0, 0};
      if (k < nagg) { agg[k].ms += ms; agg[k].n++; }
      cur = cur->next;
    }
    double total = std::chrono::duration<double, std::milli>(clk::now() - t_begin).count();
    if (total > 50) {
      fprintf(stderr, "d9mt slow chunk: %.1fms\n", total);
      for (unsigned k = 0; k < nagg; k++)
        if (agg[k].ms > 5) fprintf(stderr, "  %8.1fms n=%u %s\n", agg[k].ms, agg[k].n, agg[k].name);
      fflush(stderr);
    }
#else
    impl::CommandBase<Context> *cur = &empty;
    while (cur) {
      cur->invoke(context);
      cur = cur->next;
    }
#endif
  };
};

}; // namespace dxmt
