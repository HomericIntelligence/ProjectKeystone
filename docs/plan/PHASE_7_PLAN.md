# Phase 7: AI Integration Plan

**Status**: 📝 Planning
**Date Started**: TBD
**Target Completion**: TBD
**Branch**: TBD

## Overview

Phase 7 transforms the ProjectKeystone HMAS into an **AI-powered development system** by integrating Large Language Models (LLMs) for natural language task execution, code generation, and intelligent decision-making. The system will accept natural language goals and autonomously decompose them into executable tasks.

### Current Status (Post-Phase 5)

**What We Have**:
- ✅ Full 4-layer async hierarchy (ChiefArchitect → ComponentLead → ModuleLead → TaskAgent)
- ✅ `AsyncTaskAgent` base class for task execution
- ✅ Message passing infrastructure (MessageBus, KeystoneMessage)
- ✅ 329/329 tests passing

**What Phase 7 Adds**:
- LLM-powered task agents (`LLMTaskAgent`)
- Natural language goal parsing
- Code generation and validation pipeline
- Retrieval-Augmented Generation (RAG) for codebase context
- Prompt engineering framework
- AI-assisted task decomposition

---

## Phase 7 Architecture

```
┌─────────────────────────────────────────────────────────┐
│ User Input: "Implement a binary search tree in C++20"  │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ Level 0: ChiefArchitectAgent (AI-Enhanced)             │
│  - Natural language parsing via LLM                     │
│  - High-level task decomposition                        │
│  - Component selection                                  │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ Level 1: ComponentLeadAgent (AI-Enhanced)              │
│  - Module-level planning via LLM                        │
│  - Dependency identification                            │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ Level 2: ModuleLeadAgent (AI-Enhanced)                 │
│  - Task breakdown via LLM                               │
│  - Subtask prioritization                               │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ Level 3: LLMTaskAgent (NEW)                            │
│  ┌──────────────────────────────────────────────┐     │
│  │ LLM Integration Layer                        │     │
│  │  - OpenAI API / Anthropic Claude / Llama.cpp│     │
│  │  - Prompt engineering                        │     │
│  │  - Response parsing                          │     │
│  └──────────────────────────────────────────────┘     │
│  ┌──────────────────────────────────────────────┐     │
│  │ Code Generation Pipeline                     │     │
│  │  1. LLM generates code                       │     │
│  │  2. Static analysis (clang-tidy)             │     │
│  │  3. Compilation test                         │     │
│  │  4. Unit test generation                     │     │
│  │  5. Validation feedback loop                 │     │
│  └──────────────────────────────────────────────┘     │
│  ┌──────────────────────────────────────────────┐     │
│  │ RAG (Retrieval-Augmented Generation)         │     │
│  │  - Vector database (Chroma/Pinecone)         │     │
│  │  - Codebase embedding                        │     │
│  │  - Semantic search for context               │     │
│  └──────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────┘
```

---

## Phase 7 Implementation Plan

### Phase 7.1: LLM Integration Foundation (Week 1-2)

**Goal**: Integrate LLM APIs and create `LLMTaskAgent` class

**Tasks**:

1. **LLM Client Abstraction** (8 hours)
   ```cpp
   // include/ai/llm_client.hpp
   class LLMClient {
   public:
       virtual ~LLMClient() = default;

       struct CompletionRequest {
           std::string prompt;
           std::string system_prompt;
           double temperature = 0.7;
           int max_tokens = 2048;
           std::vector<std::string> stop_sequences;
       };

       struct CompletionResponse {
           std::string content;
           int tokens_used;
           std::string finish_reason;  // "stop", "length", "error"
       };

       virtual Task<CompletionResponse> complete(const CompletionRequest& req) = 0;
   };
   ```

2. **OpenAI API Client** (12 hours)
   ```cpp
   // include/ai/openai_client.hpp
   class OpenAIClient : public LLMClient {
   public:
       OpenAIClient(const std::string& api_key, const std::string& model = "gpt-4");

       Task<CompletionResponse> complete(const CompletionRequest& req) override;

   private:
       std::string api_key_;
       std::string model_;
       std::unique_ptr<httplib::Client> http_client_;

       std::string buildRequestJson(const CompletionRequest& req);
       CompletionResponse parseResponseJson(const std::string& json);
   };
   ```

3. **Anthropic Claude Client** (12 hours)
   ```cpp
   // include/ai/anthropic_client.hpp
   class AnthropicClient : public LLMClient {
   public:
       AnthropicClient(const std::string& api_key, const std::string& model = "claude-3-5-sonnet-20241022");

       Task<CompletionResponse> complete(const CompletionRequest& req) override;

   private:
       std::string api_key_;
       std::string model_;
       std::unique_ptr<httplib::Client> http_client_;
   };
   ```

4. **Local LLM Client (llama.cpp)** (16 hours)
   ```cpp
   // include/ai/llama_client.hpp
   class LlamaCppClient : public LLMClient {
   public:
       LlamaCppClient(const std::string& model_path);

       Task<CompletionResponse> complete(const CompletionRequest& req) override;

   private:
       std::string model_path_;
       // llama.cpp integration
   };
   ```

5. **LLMTaskAgent Class** (12 hours)
   ```cpp
   // include/agents/llm_task_agent.hpp
   class LLMTaskAgent : public AsyncTaskAgent {
   public:
       LLMTaskAgent(const std::string& agent_id,
                   std::unique_ptr<LLMClient> llm_client);

       Task<Response> processMessageAsync(const KeystoneMessage& msg) override;

   private:
       std::unique_ptr<LLMClient> llm_client_;

       Task<std::string> generateCode(const std::string& task_description);
       Task<bool> validateCode(const std::string& code);
       Task<std::string> fixCode(const std::string& code, const std::string& error);
   };
   ```

**Deliverables**:
- ✅ `LLMClient` interface
- ✅ OpenAI API client
- ✅ Anthropic Claude client
- ✅ Local LLM client (llama.cpp)
- ✅ `LLMTaskAgent` class
- ✅ Unit tests for all clients

**Estimated Time**: 60 hours

---

### Phase 7.2: Natural Language Goal Processing (Week 3)

**Goal**: Parse natural language goals into structured tasks

**Tasks**:

1. **Goal Parser** (12 hours)
   ```cpp
   // include/ai/goal_parser.hpp
   class GoalParser {
   public:
       GoalParser(std::unique_ptr<LLMClient> llm);

       struct ParsedGoal {
           std::string objective;
           std::vector<std::string> components;
           std::vector<std::string> modules;
           std::vector<std::string> tasks;
           std::map<std::string, std::vector<std::string>> dependencies;
       };

       Task<ParsedGoal> parse(const std::string& natural_language_goal);

   private:
       std::unique_ptr<LLMClient> llm_;
       std::string buildParsingPrompt(const std::string& goal);
   };
   ```

2. **Prompt Engineering Framework** (10 hours)
   ```cpp
   // include/ai/prompt_template.hpp
   class PromptTemplate {
   public:
       static std::string goalDecomposition(const std::string& goal);
       static std::string codeGeneration(const std::string& task, const std::string& context);
       static std::string codeReview(const std::string& code);
       static std::string testGeneration(const std::string& code);
       static std::string errorFix(const std::string& code, const std::string& error);

   private:
       static std::string substituteVariables(const std::string& template_str,
                                              const std::map<std::string, std::string>& vars);
   };
   ```

3. **AI-Enhanced ChiefArchitectAgent** (8 hours)
   ```cpp
   // Enhance existing AsyncChiefArchitectAgent
   class AsyncChiefArchitectAgent {
   public:
       void setGoalParser(std::unique_ptr<GoalParser> parser);

       Task<Response> processNaturalLanguageGoal(const std::string& goal);

   private:
       std::unique_ptr<GoalParser> goal_parser_;
   };
   ```

**Deliverables**:
- ✅ `GoalParser` class
- ✅ Prompt template library
- ✅ AI-enhanced ChiefArchitectAgent
- ✅ E2E test: Natural language → structured plan

**Estimated Time**: 30 hours

---

### Phase 7.3: Code Generation Pipeline (Week 4-5)

**Goal**: Generate, validate, and test code using LLMs

**Tasks**:

1. **Code Generator** (16 hours)
   ```cpp
   // include/ai/code_generator.hpp
   class CodeGenerator {
   public:
       CodeGenerator(std::unique_ptr<LLMClient> llm);

       struct GenerationContext {
           std::string task_description;
           std::string language = "C++20";
           std::vector<std::string> similar_code_examples;  // From RAG
           std::string style_guide;
       };

       Task<std::string> generateCode(const GenerationContext& ctx);

   private:
       std::unique_ptr<LLMClient> llm_;
       std::string buildCodePrompt(const GenerationContext& ctx);
   };
   ```

2. **Static Analysis Validator** (12 hours)
   ```cpp
   // include/ai/code_validator.hpp
   class CodeValidator {
   public:
       struct ValidationResult {
           bool valid;
           std::vector<std::string> errors;
           std::vector<std::string> warnings;
       };

       ValidationResult validateSyntax(const std::string& code);
       ValidationResult runClangTidy(const std::string& code);
       ValidationResult compileCode(const std::string& code);
   };
   ```

3. **Feedback Loop for Code Fixing** (12 hours)
   ```cpp
   // In LLMTaskAgent
   Task<std::string> LLMTaskAgent::generateAndValidateCode(const std::string& task) {
       int max_attempts = 3;

       for (int attempt = 0; attempt < max_attempts; ++attempt) {
           // Generate code
           auto code = co_await code_generator_->generateCode({.task_description = task});

           // Validate
           auto validation = code_validator_->validateSyntax(code);
           if (validation.valid) {
               co_return code;
           }

           // Fix using LLM
           code = co_await fixCode(code, validation.errors[0]);
       }

       throw std::runtime_error("Failed to generate valid code after max attempts");
   }
   ```

4. **Unit Test Generation** (12 hours)
   ```cpp
   // include/ai/test_generator.hpp
   class TestGenerator {
   public:
       TestGenerator(std::unique_ptr<LLMClient> llm);

       Task<std::string> generateTests(const std::string& code,
                                       const std::string& test_framework = "Catch2");

   private:
       std::unique_ptr<LLMClient> llm_;
   };
   ```

5. **Integration with LLMTaskAgent** (8 hours)
   - Wire up CodeGenerator, CodeValidator, TestGenerator
   - Implement end-to-end code generation workflow
   - Handle compilation errors and retry logic

**Deliverables**:
- ✅ `CodeGenerator` class
- ✅ `CodeValidator` class
- ✅ `TestGenerator` class
- ✅ Feedback loop for code fixing
- ✅ E2E test: Task → Generated code → Validated → Tests

**Estimated Time**: 60 hours

---

### Phase 7.4: Retrieval-Augmented Generation (RAG) (Week 6-7)

**Goal**: Provide codebase context to LLMs via semantic search

**Tasks**:

1. **Embedding Client** (12 hours)
   ```cpp
   // include/ai/embedding_client.hpp
   class EmbeddingClient {
   public:
       virtual Task<std::vector<float>> embed(const std::string& text) = 0;
   };

   class OpenAIEmbeddingClient : public EmbeddingClient {
   public:
       OpenAIEmbeddingClient(const std::string& api_key);
       Task<std::vector<float>> embed(const std::string& text) override;
   };
   ```

2. **Vector Database Integration** (16 hours)
   ```cpp
   // include/ai/vector_store.hpp
   class VectorStore {
   public:
       virtual ~VectorStore() = default;

       struct Document {
           std::string id;
           std::string content;
           std::vector<float> embedding;
           std::map<std::string, std::string> metadata;
       };

       virtual Task<void> addDocument(const Document& doc) = 0;
       virtual Task<std::vector<Document>> search(const std::vector<float>& query_embedding,
                                                  int top_k = 5) = 0;
   };

   class ChromaDBStore : public VectorStore {
       // ChromaDB C++ client integration
   };
   ```

3. **Codebase Indexer** (16 hours)
   ```cpp
   // include/ai/codebase_indexer.hpp
   class CodebaseIndexer {
   public:
       CodebaseIndexer(std::unique_ptr<EmbeddingClient> embedding_client,
                      std::unique_ptr<VectorStore> vector_store);

       Task<void> indexDirectory(const std::string& path);
       Task<void> indexFile(const std::string& file_path);

   private:
       std::vector<std::string> extractCodeChunks(const std::string& file_content);
   };
   ```

4. **Context Retriever** (12 hours)
   ```cpp
   // include/ai/context_retriever.hpp
   class ContextRetriever {
   public:
       ContextRetriever(std::unique_ptr<EmbeddingClient> embedding_client,
                       std::unique_ptr<VectorStore> vector_store);

       Task<std::string> retrieveRelevantContext(const std::string& task_description,
                                                 int max_chunks = 5);

   private:
       std::unique_ptr<EmbeddingClient> embedding_client_;
       std::unique_ptr<VectorStore> vector_store_;
   };
   ```

5. **RAG Integration with CodeGenerator** (8 hours)
   ```cpp
   // Enhance CodeGenerator to use RAG
   Task<std::string> CodeGenerator::generateCode(const GenerationContext& ctx) {
       // Retrieve relevant code examples
       auto context = co_await context_retriever_->retrieveRelevantContext(ctx.task_description);

       // Build prompt with retrieved context
       auto prompt = buildCodePromptWithRAG(ctx, context);

       // Generate code
       auto response = co_await llm_->complete({.prompt = prompt});
       co_return response.content;
   }
   ```

**Deliverables**:
- ✅ `EmbeddingClient` interface
- ✅ OpenAI embedding client
- ✅ Vector database integration (ChromaDB or Pinecone)
- ✅ Codebase indexer
- ✅ Context retriever
- ✅ RAG-enhanced code generation

**Estimated Time**: 64 hours

---

### Phase 7.5: E2E AI-Powered Workflow (Week 8)

**Goal**: End-to-end test of AI-powered HMAS

**Tasks**:

1. **E2E Test: Natural Language Goal → Code** (12 hours)
   ```cpp
   TEST_CASE("AI-Powered HMAS: Binary Search Tree Implementation", "[e2e][ai]") {
       // Setup
       auto openai_client = std::make_unique<OpenAIClient>(api_key);
       auto llm_task_agent = std::make_unique<LLMTaskAgent>("llm_task_1", std::move(openai_client));

       auto goal_parser = std::make_unique<GoalParser>(std::make_unique<OpenAIClient>(api_key));
       auto chief = std::make_unique<AsyncChiefArchitectAgent>("chief");
       chief->setGoalParser(std::move(goal_parser));

       // Natural language goal
       std::string goal = "Implement a binary search tree in C++20 with insert, search, and delete methods";

       // Process goal
       auto response = co_await chief->processNaturalLanguageGoal(goal);

       // Verify code generated
       REQUIRE(response.status == "completed");
       REQUIRE(response.payload.contains("code"));

       std::string generated_code = response.payload["code"];
       REQUIRE(generated_code.find("class BinarySearchTree") != std::string::npos);
       REQUIRE(generated_code.find("void insert") != std::string::npos);
       REQUIRE(generated_code.find("bool search") != std::string::npos);
   }
   ```

2. **E2E Test: RAG-Enhanced Code Generation** (8 hours)
   - Index existing codebase (e.g., AsyncBaseAgent, MessageBus)
   - Task: "Implement a new agent similar to AsyncTaskAgent"
   - Verify RAG retrieves relevant context
   - Verify generated code uses similar patterns

3. **E2E Test: Code Validation and Fixing** (8 hours)
   - Inject intentional syntax error in prompt
   - Verify LLMTaskAgent detects error
   - Verify LLMTaskAgent fixes error via feedback loop
   - Verify final code compiles

4. **Performance Benchmarking** (8 hours)
   - Measure LLM API latency (p50, p95, p99)
   - Measure code generation time
   - Measure RAG retrieval time
   - Identify optimization opportunities

**Deliverables**:
- ✅ E2E test: Natural language → Code
- ✅ E2E test: RAG-enhanced generation
- ✅ E2E test: Validation and fixing
- ✅ Performance benchmarks

**Estimated Time**: 36 hours

---

## Success Criteria

### Must Have ✅
- [ ] LLM client abstraction working (OpenAI + Anthropic)
- [ ] `LLMTaskAgent` class implemented
- [ ] Natural language goal parsing working
- [ ] Code generation pipeline functional
- [ ] Code validation and feedback loop working
- [ ] RAG integration complete
- [ ] E2E test: Natural language → Generated code

### Should Have 🎯
- [ ] Local LLM support (llama.cpp)
- [ ] Unit test generation via LLM
- [ ] Multi-turn code fixing (3+ attempts)
- [ ] Codebase indexing and semantic search
- [ ] RAG-enhanced code generation
- [ ] Performance benchmarks (LLM latency, generation time)

### Nice to Have 💡
- [ ] Multi-LLM routing (use GPT-4 for planning, GPT-3.5 for simple tasks)
- [ ] Cost optimization (token usage tracking, caching)
- [ ] Fine-tuned models for C++20 code generation
- [ ] Code review agent (LLM critiques generated code)
- [ ] Conversational debugging (LLM helps debug errors)

---

## Test Plan

### E2E Tests (Phase 7)

1. **NaturalLanguageGoalProcessing** (Priority: CRITICAL)
   - Input: "Implement a stack data structure"
   - Verify goal parsed into components/modules/tasks
   - Verify structured plan returned

2. **CodeGenerationPipeline** (Priority: CRITICAL)
   - Input: "Implement stack::push() method"
   - Verify code generated by LLM
   - Verify code compiles
   - Verify code passes basic tests

3. **CodeValidationAndFixing** (Priority: HIGH)
   - Inject syntax error in generated code
   - Verify validator detects error
   - Verify LLM fixes error
   - Verify fixed code compiles

4. **RAGEnhancedGeneration** (Priority: HIGH)
   - Index sample codebase
   - Task: "Implement agent similar to TaskAgent"
   - Verify RAG retrieves relevant code
   - Verify generated code uses similar patterns

5. **MultiTurnCodeGeneration** (Priority: MEDIUM)
   - Task with intentional ambiguity
   - Verify LLM asks clarifying questions
   - Verify multi-turn conversation works
   - Verify final code meets requirements

---

## Performance Expectations

**LLM API Latency**:
- OpenAI GPT-4: ~5-10 seconds (p95)
- Anthropic Claude: ~3-7 seconds (p95)
- Local LLM (llama.cpp): ~1-3 seconds (p95, on GPU)

**Code Generation Time** (end-to-end):
- Simple task (< 50 lines): ~10-20 seconds
- Medium task (50-200 lines): ~30-60 seconds
- Complex task (200+ lines): ~60-120 seconds

**RAG Retrieval Time**:
- Embedding generation: ~100-500 ms
- Vector search (top 5): ~50-200 ms
- Total RAG overhead: ~500 ms

---

## Risk Mitigation

### Risk 1: LLM API Costs
**Impact**: High
**Likelihood**: High
**Mitigation**: Token usage tracking, caching, use cheaper models for simple tasks.

### Risk 2: LLM Hallucinations (Invalid Code)
**Impact**: High
**Likelihood**: Medium
**Mitigation**: Validation pipeline with static analysis, compilation, tests. Feedback loop for fixing.

### Risk 3: LLM API Latency
**Impact**: Medium
**Likelihood**: Medium
**Mitigation**: Async execution, caching, local LLM option.

### Risk 4: RAG Retrieval Quality
**Impact**: Medium
**Likelihood**: Medium
**Mitigation**: Experiment with chunking strategies, embedding models, top-k selection.

---

## Dependencies

### New Dependencies

1. **cpp-httplib** - HTTP client for API calls
2. **nlohmann/json** - JSON parsing
3. **OpenAI C++ SDK** (or custom HTTP client)
4. **Anthropic C++ SDK** (or custom HTTP client)
5. **llama.cpp** - Local LLM inference (optional)
6. **ChromaDB C++ client** - Vector database
7. **Pinecone C++ client** - Alternative vector database

---

## Next Steps

1. **Week 1-2**: LLM integration foundation
2. **Week 3**: Natural language goal processing
3. **Week 4-5**: Code generation pipeline
4. **Week 6-7**: RAG integration
5. **Week 8**: E2E AI-powered workflow

**After Phase 7**: Move to **Phase 8: Real Distributed System** or **Phase 10: Advanced Features**

---

**Status**: 📝 Planning - Ready for Implementation
**Total Estimated Time**: 250 hours (~8 weeks)
**Last Updated**: 2025-11-19
