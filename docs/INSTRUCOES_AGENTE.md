# Instruções para desenvolvimento

## 1. Regra zero: leia antes de agir

Antes de analisar, planejar, escrever código ou alterar qualquer arquivo:

1. Leia todos os requisitos fornecidos pelo usuário.
2. Leia as instruções do projeto e os arquivos de contexto relevantes.
3. Leia o `ARCHITECTURE.md` e o `IMPLEMENTATION_PLAN.md`, além de outros arquivos de instrução quando existir.
4. Examine apenas o código e os testes necessários para entender a tarefa.
5. Confirme o objetivo, as restrições e os critérios de aceitação.

Não comece a implementar enquanto houver uma ambiguidade capaz de alterar a solução. Diga claramente o que está confuso e peça esclarecimento.

Se duas instruções entrarem em conflito, exponha o conflito antes de continuar. Não escolha silenciosamente.

## 2. Pense antes de programar

Antes de implementar:

- Declare explicitamente as suposições relevantes.
- Apresente interpretações alternativas quando houver mais de uma leitura possível.
- Mostre os principais trade-offs da solução.
- Aponte uma alternativa mais simples quando ela atender aos mesmos requisitos.
- Questione requisitos que adicionem complexidade sem benefício claro.
- Defina critérios de sucesso objetivos e verificáveis.

Não esconda dúvidas. Se uma informação ausente puder mudar a implementação, pare e pergunte.

## 3. Crie e mantenha o plano de implementação

Para toda tarefa com mais de uma etapa, crie ou atualize o `IMPLEMENTATION_PLAN.md` antes de alterar o código.

O plano deve conter, no mínimo:

- objetivo da tarefa;
- requisitos e critérios de aceitação;
- suposições confirmadas;
- etapas de implementação;
- verificação prevista para cada etapa;
- tarefas pendentes, em andamento e concluídas;
- bugs encontrados e seus estados;
- testes executados e respectivos resultados;
- bloqueios e decisões ainda necessárias.

Use etapas curtas e verificáveis:

```text
1. [Etapa] -> verificar: [checagem objetiva]
2. [Etapa] -> verificar: [checagem objetiva]
3. [Etapa] -> verificar: [checagem objetiva]
```

O plano é um registro vivo. Atualize-o durante o trabalho, não apenas no início ou no final.

Para uma alteração realmente trivial e de etapa única, apresente o objetivo e a verificação na própria resposta; não crie arquivos de controle desnecessários.

## 4. Priorize a simplicidade

Implemente o mínimo necessário para atender integralmente aos requisitos.

- Não adicione funcionalidades que não foram solicitadas.
- Não crie abstrações para código de uso único.
- Não introduza flexibilidade ou configuração especulativa.
- Não trate cenários impossíveis dentro das restrições definidas.
- Não aumente o escopo sem autorização.
- Se uma solução ficou maior ou mais complexa do que o necessário, simplifique-a antes de concluir.

Pergunta obrigatória durante a revisão: **um engenheiro experiente consideraria esta solução desnecessariamente complexa?** Se a resposta for sim, simplifique.

## 5. Faça alterações cirúrgicas

Altere somente o que for necessário para cumprir a tarefa.

- Não reformate, reorganize ou melhore código adjacente sem necessidade.
- Não refatore partes que não estão relacionadas ao problema.
- Preserve o estilo e os padrões existentes do projeto.
- Não remova código morto preexistente sem solicitação; apenas registre sua existência.
- Remova imports, variáveis, funções e arquivos que tenham se tornado desnecessários exclusivamente por causa das suas próprias alterações.
- Preserve mudanças existentes do usuário e evite sobrescrevê-las.

Cada linha alterada deve ter relação direta com um requisito ou com a correção de um problema introduzido pela própria mudança.

## 6. Execute de forma orientada a objetivos

Transforme cada pedido em um resultado verificável:

- **Adicionar validação** -> criar testes para entradas inválidas e fazê-los passar.
- **Corrigir um bug** -> reproduzir o problema, registrar a falha, implementar a correção e confirmar que ela não reaparece.
- **Refatorar** -> garantir que os testes relevantes passem antes e depois da alteração.
- **Adicionar funcionalidade** -> relacionar cada critério de aceitação a pelo menos uma verificação.

Não considere uma tarefa concluída apenas porque o código foi escrito.

## 7. Ciclo obrigatório de implementação

Repita este ciclo enquanto houver tarefas pendentes, porém a cada 20 ciclos, dê atualizações e aguarde ordens de continuar:

1. Releia o requisito e a etapa atual do plano.
2. Implemente a menor mudança capaz de atender à etapa.
3. Revise todas as alterações realizadas.
4. Verifique o diff e confirme que não existem mudanças fora do escopo.
5. Execute os testes aplicáveis.
6. Registre os resultados dos testes.
7. Registre qualquer bug encontrado.
8. Corrija os bugs relacionados à tarefa.
9. Execute novamente os testes afetados e os testes de regressão possíveis.
10. Atualize o plano e avance para a próxima tarefa pendente.

Continue até que todas as tarefas estejam concluídas ou exista um bloqueio que dependa do usuário ou de acesso externo.

## 8. Registro de bugs

Todo bug encontrado durante a execução deve ser registrado no `IMPLEMENTATION_PLAN.md` ou no sistema de acompanhamento já adotado pelo projeto.

Registre, no mínimo:

- descrição objetiva;
- forma de reprodução;
- comportamento esperado e comportamento observado;
- impacto;
- relação com a tarefa atual;
- estado: `aberto`, `corrigido`, `bloqueado` ou `fora do escopo`;
- evidência da correção e do novo teste, quando aplicável.

Corrija bugs causados pelas próprias alterações antes de finalizar. Para bugs preexistentes e fora do escopo, não altere o código sem autorização; registre-os e informe o usuário.

## 9. Testes e verificação

Execute todos os testes razoavelmente possíveis no ambiente disponível, priorizando:

1. teste que reproduz o problema ou valida o novo comportamento;
2. testes diretamente afetados;
3. testes de regressão do módulo alterado;
4. análise estática, formatação ou build exigidos pelo projeto;
5. verificação manual, quando a automação não for possível.

Nunca afirme que algo foi testado sem tê-lo executado. Registre o comando ou procedimento utilizado, o resultado e qualquer limitação.

Se um teste não puder ser executado, explique exatamente por quê e indique o que ainda precisa ser validado.

## 10. Revisão final obrigatória

Antes de concluir:

- releia os requisitos e o plano de implementação;
- confira cada critério de aceitação;
- revise todo o diff produzido;
- confirme que as mudanças estão dentro do escopo;
- procure bugs, regressões, código desnecessário e complexidade evitável;
- confirme que os testes relevantes passaram;
- atualize o estado de todas as tarefas e bugs;
- registre limitações, riscos e validações pendentes.

Se a revisão encontrar um problema, volte ao ciclo de implementação. Não finalize com tarefas conhecidas ainda pendentes.

## 11. Critérios para finalização

Uma tarefa só pode ser declarada concluída quando:

- todos os requisitos aplicáveis foram atendidos;
- todos os critérios de aceitação foram verificados;
- todas as etapas do plano estão concluídas;
- não existem bugs relacionados à mudança ainda abertos;
- os testes possíveis foram executados e registrados;
- o código e o diff foram revisados;
- bloqueios, limitações e itens fora do escopo foram informados claramente.

Se algo depender do usuário, de hardware, de credenciais ou de um serviço indisponível, registre o bloqueio e solicite a ação necessária. Não marque a tarefa como concluída e não invente resultados.

## 12. Relatório de entrega

Ao finalizar, informe de forma objetiva:

- o que foi implementado;
- quais arquivos foram alterados;
- quais testes foram executados e seus resultados;
- quais bugs foram encontrados e corrigidos;
- quais limitações, riscos ou pendências permanecem.

O relatório deve refletir o estado real do trabalho, sem omitir falhas ou validações não realizadas.
