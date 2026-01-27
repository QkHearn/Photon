---
name: create-subagent
description: Photon Subagent Creator: Create custom subagents for specialized AI tasks with isolated contexts.
---

# Skill: Photon Subagent Creator

This skill guides you through creating custom subagents for Photon. Subagents are specialized AI assistants that run in isolated contexts with custom system prompts.

## When to Use Subagents
Subagents help you:
- **Preserve context** by isolating exploration from your main conversation.
- **Specialize behavior** with focused system prompts for specific domains.
- **Reuse configurations** across projects.

## Subagent Locations
| Location | Scope | Priority |
|----------|-------|----------|
| `.photon/agents/` | Current project | Higher |
| `~/.photon/agents/` | All your projects | Lower |

## Subagent File Format
Create a `.md` file with YAML frontmatter:
```markdown
---
name: code-reviewer
description: Reviews code for quality and best practices
---
You are a code reviewer. Analyze the code and provide actionable feedback.
```
