# Guía de Contribución

## Política de Branching (Gitflow)

Se implementa el flujo de trabajo **Gitflow** para estructurar la colaboración y el ciclo de vida del código, garantizando que solo el código estable llegue a producción.

### Ramas principales

| Rama | Propósito |
|---|---|
| `main` | Contiene el código productivo y estable. Se utiliza exclusivamente para el despliegue a Producción. Solo se actualiza mediante merges desde `develop` (o desde una rama `hotfix/` en casos urgentes). |
| `develop` | Rama de integración principal. Aquí se consolidan todas las nuevas funcionalidades terminadas. |

### Ramas temporales

| Rama | Propósito | Origen → Destino | Ejemplo |
|---|---|---|---|
| `feature/[ID-Jira]-[descripción]` | Desarrollo de nuevas funcionalidades. | `develop` → `develop` | `feature/TV-45-login-auth` |
| `fix/[ID-Jira]-[descripción]` | Corrección de errores detectados en Pruebas (QA). | `develop` → `develop` | `fix/TV-52-error-validacion-formulario` |
| `hotfix/[descripción]` | Correcciones urgentes que deben ir directo a producción para evitar la interrupción del servicio. | `main` → `main` y `develop` | `hotfix/error-caida-login` |

### Reglas generales

- `main` **nunca** se actualiza directamente: solo recibe merges desde `develop` o desde una rama `hotfix/`.
- Toda rama `feature/` o `fix/` parte de `develop` y se integra nuevamente en `develop` (vía Pull Request).
- Toda rama `hotfix/` parte de `main`, se mergea a `main` para el despliegue urgente y luego se hace back-merge a `develop` para que el fix no se pierda en la siguiente release.
- El nombre de la rama debe incluir el ID de Jira cuando corresponda (`feature/` y `fix/`), para trazabilidad entre el código y el ticket.
