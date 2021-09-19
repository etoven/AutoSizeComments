#include "AutoSizeCommentsGraphHandler.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditAction.h"
#include "SGraphPanel.h"

void FAutoSizeCommentGraphHandler::BindDelegates()
{
	bPendingSave = false;
	FCoreUObjectDelegates::OnObjectSaved.AddRaw(this, &FAutoSizeCommentGraphHandler::OnObjectSaved);
	FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FAutoSizeCommentGraphHandler::OnObjectTransacted);
}

void FAutoSizeCommentGraphHandler::UnbindDelegates()
{
	FCoreUObjectDelegates::OnObjectSaved.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);

	for (const auto& Kvp: GraphHandles)
	{
		if (Kvp.Key.IsValid())
		{
			Kvp.Key->RemoveOnGraphChangedHandler(Kvp.Value);
		}
	}

	GraphHandles.Empty();
}

void FAutoSizeCommentGraphHandler::BindToGraph(UEdGraph* Graph)
{
	if (!Graph || GraphHandles.Contains(Graph))
	{
		return;
	}

	const FDelegateHandle Handle = Graph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::OnGraphChanged));
	GraphHandles.Add(Graph, Handle);
}

void FAutoSizeCommentGraphHandler::OnGraphChanged(const FEdGraphEditAction& Action)
{
	if (Action.Action != GRAPHACTION_AddNode)
	{
		return;
	}

	if (Action.Nodes.Num() != 1)
	{
		return;
	}

	const UEdGraphNode* ConstNewNode = Action.Nodes.Array()[0];
	if (ConstNewNode->Pins.Num() == 0)
	{
		return;
	}

	// we don't want a const prt
	UEdGraphNode* NewNode = ConstNewNode->Pins[0]->GetOwningNode();

	TArray<UEdGraphNode_Comment*> Comments;
	NewNode->GetGraph()->GetNodesOfClassEx<UEdGraphNode_Comment>(Comments);
	if (Comments.Num() == 0)
	{
		return;
	}

	FASCState& State = IAutoSizeCommentsModule::Get().GetState();
	TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = State.GetASCComment(Comments[0]);
	if (!ASCComment.IsValid())
	{
		return;
	}

	TSharedPtr<SGraphPanel> OwnerPanel = ASCComment->GetOwnerPanel();
	if (!OwnerPanel || !OwnerPanel->SelectionManager.SelectedNodes.Num() == 1)
	{
		return;
	}

	UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(OwnerPanel->SelectionManager.SelectedNodes.Array()[0]);

	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::AutoInsertIntoCommentNodes, NewNode, SelectedNode));
}

void FAutoSizeCommentGraphHandler::AutoInsertIntoCommentNodes(UEdGraphNode* NewNode, UEdGraphNode* LastSelectedNode)
{
	UEdGraph* Graph = NewNode->GetGraph();
	const auto IsSelectedNode = [&LastSelectedNode](UEdGraphNode* LinkedNode) { return LinkedNode == LastSelectedNode; };
	TArray<UEdGraphNode*> LinkedInput = FASCUtils::GetLinkedNodes(NewNode, EGPD_Input).FilterByPredicate(IsSelectedNode);
	TArray<UEdGraphNode*> LinkedOutput = FASCUtils::GetLinkedNodes(NewNode, EGPD_Output).FilterByPredicate(IsSelectedNode);

	struct FLocal
	{
		static void TakeCommentNode(UEdGraph* Graph, UEdGraphNode* Node, UEdGraphNode* NodeToTakeFrom)
		{
			TArray<UEdGraphNode_Comment*> CommentNodes;
			Graph->GetNodesOfClass(CommentNodes);
			auto ContainingComments = FASCUtils::GetContainingCommentNodes(CommentNodes, NodeToTakeFrom);
			for (UEdGraphNode_Comment* CommentNode : ContainingComments)
			{
				CommentNode->AddNodeUnderComment(Node);
			}
		};
	};

	const auto AutoInsertStyle = GetDefault<UAutoSizeCommentsSettings>()->AutoInsertComment;
	if (AutoInsertStyle == EASCAutoInsertComment::Surrounded)
	{
		if (LinkedInput.Num() == 1 && LinkedOutput.Num() == 1)
		{
			TArray<UEdGraphNode_Comment*> CommentNodes;
			Graph->GetNodesOfClass(CommentNodes);
			auto ContainingCommentsA = FASCUtils::GetContainingCommentNodes(CommentNodes, LinkedOutput[0]);
			auto ContainingCommentsB = FASCUtils::GetContainingCommentNodes(CommentNodes, LinkedInput[0]);
	
			ContainingCommentsA.RemoveAll([&ContainingCommentsB](UEdGraphNode_Comment* Comment)
			{
				return !ContainingCommentsB.Contains(Comment);
			});
	
			if (ContainingCommentsA.Num() > 0)
			{
				FLocal::TakeCommentNode(Graph, NewNode, ContainingCommentsA[0]);
			}
		}
	}
	else if (AutoInsertStyle == EASCAutoInsertComment::Always)
	{
		if (LinkedOutput.Num() == 1)
		{
			FLocal::TakeCommentNode(Graph, NewNode, LinkedOutput[0]);
		}
	
		if (LinkedInput.Num() == 1)
		{
			FLocal::TakeCommentNode(Graph, NewNode, LinkedInput[0]);
		}
	}
}

void FAutoSizeCommentGraphHandler::OnObjectSaved(UObject* Object)
{
	FAutoSizeCommentsCacheFile& SizeCache = IAutoSizeCommentsModule::Get().GetSizeCache();

	// upon saving a graph, save all comments to cache
	if (UEdGraph* Graph = Cast<UEdGraph>(Object))
	{
		TArray<UEdGraphNode_Comment*> Comments;
		Graph->GetNodesOfClassEx<UEdGraphNode_Comment>(Comments);

		for (UEdGraphNode_Comment* Comment : Comments)
		{
			SizeCache.UpdateComment(Comment);
		}

		if (!bPendingSave)
		{
			bPendingSave = true;
			GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::SaveSizeCache));
		}
	}
}

void FAutoSizeCommentGraphHandler::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (Event.GetEventType() != ETransactionObjectEventType::UndoRedo && Event.GetEventType() != ETransactionObjectEventType::Finalized)
	{
		return;
	}

	if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::UpdateContainingComments, Node));
	}
}

void FAutoSizeCommentGraphHandler::SaveSizeCache()
{
	IAutoSizeCommentsModule::Get().GetSizeCache().SaveCache();
	bPendingSave = false;
}

void FAutoSizeCommentGraphHandler::UpdateContainingComments(UEdGraphNode* Node)
{
	FASCState& State = IAutoSizeCommentsModule::Get().GetState();

	if (!IsValid(Node))
	{
		return;
	}

	UEdGraph* Graph = Node->GetGraph();
	if (!IsValid(Graph))
	{
		return;
	}

	TArray<UEdGraphNode_Comment*> Comments;

	// check if any node on the graph contains the new node
	Graph->GetNodesOfClass<UEdGraphNode_Comment>(Comments);
	for (UEdGraphNode_Comment* Comment : Comments)
	{
		if (Comment->GetNodesUnderComment().Contains(Node))
		{
			if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = State.GetASCComment(Comment))
			{
				ASCComment->ResizeToFit();
			}
		}
	}
}