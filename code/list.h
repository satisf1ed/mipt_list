#include <iostream>

template <size_t N>
struct StackStorage {
  char stor[N];
  size_t start{0};

  StackStorage(const StackStorage&) = delete;

  StackStorage() = default;

  ~StackStorage() = default;

  StackStorage& operator=(const StackStorage&) = delete;
};

template <typename T, size_t N>
struct StackAllocator {
  StackStorage<N>* stor{nullptr};
  size_t capacity{N - stor->start};

  StackAllocator() = default;

  StackAllocator(StackStorage<N>& storage)
    : stor(&storage), capacity(N - stor->start) {}

  template <typename U>
  StackAllocator(const StackAllocator<U, N>& other)
    : stor(other.stor), capacity(other.capacity) {}

  ~StackAllocator() = default;

  template <typename U>
  StackAllocator& operator=(const StackAllocator<U, N>& other) {
    StackAllocator copy = other;
    std::swap(stor, copy.stor);
    std::swap(capacity, copy.capacity);
  }

  T* allocate(size_t count) {
    size_t ptr = stor->start;
    unsigned long mod =
      reinterpret_cast<intptr_t>(stor->stor + stor->start) % alignof(T);
    if (mod != 0) {
      stor->start += alignof(T) - mod;
    }
    auto to_paste = stor->stor + stor->start;
    stor->start += sizeof(T) * count;
    if (stor->start >= N) {
      stor->start = ptr;
      throw std::runtime_error("");
    }
    return reinterpret_cast<T*>(to_paste);
  }

  void deallocate(T* /*unused*/, size_t /*unused*/) {}

  // NOLINTBEGIN
  template <typename U>
  struct [[maybe_unused]] rebind {
    using other = StackAllocator<U, N>;
  };
  // NOLINTEND

  bool operator==(const StackAllocator& other) { return stor == other.stor; }

  bool operator!=(const StackAllocator& other) { return stor != other.stor; }

 public:
  using value_type = T;
};

template <typename T, typename Allocator = std::allocator<T>>
class List {
 public:
  struct BaseNode;
  struct Node;

  using node_alloc =
    typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;

  struct Node : BaseNode {
    T node_value;

    Node() = default;

    Node(const T& value) : node_value(value) {}
  };

  struct BaseNode {
    Node* next;
    Node* previous;

    BaseNode(Node* left = nullptr, Node* right = nullptr)
      : next(left), previous(right) {}
  };

  List() = default;

  explicit List(const Allocator& allocator) : node_alloc_(allocator) {}

  List(int size, const Allocator& allocator = Allocator())
    : node_alloc_(allocator) {
    try {
      for (int i = 0; i < size; ++i) {
        push_back();
      }
    } catch (...) {
      this->~List();
    }
  }

  List(int size, const T& value, const Allocator& allocator = Allocator())
    : node_alloc_(allocator) {
    try {
      for (int i = 0; i < size; ++i) {
        push_back(value);
      }
    } catch (...) {
      this->~List();
    }
  }

  List(const List& other)
    : node_alloc_(
    std::allocator_traits<node_alloc>::
    select_on_container_copy_construction(other.node_alloc_)),
      fakeNode_(other.fakeNode_) {
    Node* tmp_other = other.start_;
    try {
      for (size_t i = 0; i < other.size(); ++i) {
        push_back(tmp_other->node_value);
        tmp_other = tmp_other->next;
      }
    } catch (...) {
      this->~List();
    }
  }

  List& operator=(const List& lst) {
    List other(lst);
    if (std::allocator_traits<
      node_alloc>::propagate_on_container_copy_assignment::value) {
      other.node_alloc_ = lst.node_alloc_;
    }
    std::swap(start_, other.start_);
    std::swap(end_, other.end_);
    std::swap(capacity_, other.capacity_);
    std::swap(fakeNode_, other.fakeNode_);
    std::swap(node_alloc_, other.node_alloc_);
    return *this;
  }

  ~List() {
    while (size()) {
      pop_front();
    }
  }

  void push_back(const T& value) {
    try {
      push_before(end_, value);
    } catch (...) {
      throw;
    }
  }

  void push_front(const T& value) { push_before(start_, value); }

  void pop_front() {
    auto next(start_->next);
    std::allocator_traits<node_alloc>::destroy(node_alloc_, start_);
    std::allocator_traits<node_alloc>::deallocate(node_alloc_, start_, 1);
    start_ = next;
    --capacity_;
  }

  void pop_back() {
    auto to_remove(end_->previous);
    end_->previous = end_->previous->previous;
    end_->previous->next = end_;
    std::allocator_traits<node_alloc>::destroy(node_alloc_, to_remove);
    std::allocator_traits<node_alloc>::deallocate(node_alloc_, to_remove, 1);
    --capacity_;
  }

  node_alloc get_allocator() const noexcept { return node_alloc_; }

  size_t size() { return capacity_; }

  size_t size() const { return capacity_; }

  template <bool IsConst>
  class BaseIterator {
   private:
    using stor = typename std::conditional<IsConst, const Node*, Node*>::type;
    stor iter_;

   public:
    using value_type = std::conditional_t<IsConst, const T, T>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::bidirectional_iterator_tag;

    BaseIterator(Node* new_iter) : iter_(new_iter) {}

    BaseIterator(const Node* new_iter) : iter_(new_iter) {}

    operator BaseIterator<true>() const { return BaseIterator<true>(iter_); }

    BaseIterator& operator=(const BaseIterator& other) = default;

    ~BaseIterator() = default;

    value_type& operator*() { return iter_->node_value; }

    value_type* operator->() { &(iter_->node_value); }

    BaseIterator& operator++() {
      iter_ = iter_->next;
      return *this;
    }

    BaseIterator& operator--() {
      iter_ = iter_->previous;
      return *this;
    }

    BaseIterator operator--(int) {
      BaseIterator copy = *this;
      iter_ = iter_->previous;
      return copy;
    }

    BaseIterator operator++(int) {
      BaseIterator copy = *this;
      iter_ = iter_->next;
      return copy;
    }

    bool operator==(const BaseIterator& other) const {
      return (iter_ == other.iter_);
    }

    bool operator!=(const BaseIterator& other) const {
      return (iter_ != other.iter_);
    }

    stor get_pointer() { return iter_; }
  };

  using const_iterator = BaseIterator<true>;
  using iterator = BaseIterator<false>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() { return iterator(start_); }

  iterator end() { return iterator(end_); }

  const_iterator begin() const { return const_iterator(start_); }

  const_iterator end() const { return const_iterator(end_); }

  const_iterator cbegin() const { return const_iterator(start_); }

  const_iterator cend() const { return const_iterator(end_); }

  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }

  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }

  const_reverse_iterator rbegin() const {
    return std::make_reverse_iterator(cend());
  }

  const_reverse_iterator rend() const {
    return std::make_reverse_iterator(cbegin());
  }

  const_reverse_iterator crbegin() {
    return std::make_reverse_iterator(cend());
  }

  const_reverse_iterator crend() {
    return std::make_reverse_iterator(cbegin());
  }

  void insert(const_iterator iter, const T& value) {
    Node* ptr = const_cast<Node*>(iter.get_pointer());
    push_before(ptr, value);
  }

  void insert(iterator iter, const T& value) {
    auto ptr = iter.get_pointer();
    push_before(ptr, value);
  }

  void erase(const_iterator iter) {
    auto ptr = const_cast<Node*>(iter.get_pointer());
    if (ptr == start_) {
      pop_front();
    } else if (ptr == end_->previous) {
      pop_back();
    } else {
      auto to_remove(ptr);
      ptr->previous->next = ptr->next;
      ptr->next->previous = ptr->previous;
      std::allocator_traits<node_alloc>::destroy(node_alloc_, to_remove);
      std::allocator_traits<node_alloc>::deallocate(node_alloc_, to_remove, 1);
      --capacity_;
    }
  }

  void erase(iterator iter) {
    auto ptr = const_cast<Node*>(iter.get_pointer());
    if (ptr == start_) {
      pop_front();
    } else if (ptr == end_->previous) {
      pop_back();
    } else {
      auto to_remove(ptr);
      ptr->previous->next = ptr->next;
      ptr->next->previous = ptr->previous;
      std::allocator_traits<node_alloc>::destroy(node_alloc_, to_remove);
      std::allocator_traits<node_alloc>::deallocate(node_alloc_, to_remove, 1);
      --capacity_;
    }
  }

 private:
  [[no_unique_address]] node_alloc node_alloc_;
  BaseNode fakeNode_;
  Node* start_{static_cast<Node*>(&fakeNode_)};
  Node* end_{static_cast<Node*>(&fakeNode_)};
  size_t capacity_{0};

  void push_back() {
    Node* node;
    try {
      node = std::allocator_traits<node_alloc>::allocate(node_alloc_, 1);
      std::allocator_traits<node_alloc>::construct(node_alloc_, node);
    } catch (...) {
      std::allocator_traits<node_alloc>::deallocate(node_alloc_, node, 1);
      throw;
    }
    node->next = end_;
    if (start_ == &fakeNode_) {
      start_ = node;
      end_->previous = node;
    } else {
      node->previous = end_->previous;
      end_->previous = node;
      node->previous->next = node;
    }
    ++capacity_;
  }

  void push_before(Node* node_after, const T& value) {
    Node* node;
    try {
      node = std::allocator_traits<node_alloc>::allocate(node_alloc_, 1);
      std::allocator_traits<node_alloc>::construct(node_alloc_, node, value);
    } catch (...) {
      std::allocator_traits<node_alloc>::deallocate(node_alloc_, node, 1);
      throw;
    }
    node->next = node_after;
    if (start_ == &fakeNode_) {
      start_ = node;
      node_after->previous = node;
    } else {
      node->previous = node_after->previous;
      node_after->previous = node;
      if (node->previous != nullptr) {
        node->previous->next = node;
      }
    }
    if (node_after == start_) {
      start_ = node;
    }
    ++capacity_;
  }
};
